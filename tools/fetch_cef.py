#!/usr/bin/env python3
"""Fetch only a reviewed, checksum-pinned CEF archive into the workspace."""

import argparse
import hashlib
import json
import os
import platform
import re
import tarfile
import tempfile
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PIN_PATH = ROOT / "cef" / "pin.json"
DOWNLOAD_ORIGIN = "https://cef-builds.spotifycdn.com/"


def validate_pin(pin: dict) -> None:
    if (
        not isinstance(pin, dict)
        or type(pin.get("schemaVersion")) is not int
        or pin["schemaVersion"] != 1
        or pin.get("channel") != "stable"
    ):
        raise ValueError("Expected a stable CEF pin with schemaVersion 1")
    version = pin.get("cefVersion")
    match = (
        re.fullmatch(
            r"[0-9]+\.[0-9]+\.[0-9]+\+g([0-9a-f]{7,40})"
            r"\+chromium-([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)",
            version,
        )
        if isinstance(version, str)
        else None
    )
    commit = pin.get("cefCommit")
    if (
        not match
        or not isinstance(commit, str)
        or not re.fullmatch(r"[0-9a-f]{40}", commit)
        or not commit.startswith(match[1])
        or pin.get("chromiumVersion") != match[2]
    ):
        raise ValueError("CEF version, commit and Chromium version must agree")
    artifacts = pin.get("artifacts")
    if not isinstance(artifacts, dict) or set(artifacts) != {"macosarm64", "macosx64"}:
        raise ValueError("Expected the pinned macOS arm64 and x64 artifacts")
    for selected_platform, artifact in artifacts.items():
        expected_filename = f"cef_binary_{version}_{selected_platform}_minimal.tar.bz2"
        if not isinstance(artifact, dict) or artifact.get("filename") != expected_filename:
            # An exact canonical filename is also a basename: no path segments,
            # separators, encoded aliases, or URL query can escape the workspace.
            raise ValueError(f"Non-canonical CEF artifact filename: {selected_platform}")
        digest = artifact.get("sha256")
        if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise ValueError(f"Expected a SHA-256 digest: {selected_platform}")
        size = artifact.get("size")
        if type(size) is not int or not 0 < size <= 1024 * 1024 * 1024:
            raise ValueError(f"Invalid CEF archive size: {selected_platform}")


def native_platform() -> str:
    if platform.system() != "Darwin":
        raise ValueError("The first CEF preview targets macOS only")
    machine = platform.machine()
    if machine == "arm64":
        return "macosarm64"
    if machine == "x86_64":
        return "macosx64"
    raise ValueError(f"Unsupported macOS architecture: {machine}")


def verify_archive(path: Path, expected_sha256: str, expected_size: int) -> None:
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as archive:
        for chunk in iter(lambda: archive.read(1024 * 1024), b""):
            digest.update(chunk)
            size += len(chunk)
    if size != expected_size:
        raise ValueError(f"CEF archive size mismatch: expected {expected_size}, got {size}")
    if digest.hexdigest() != expected_sha256:
        raise ValueError("CEF archive SHA-256 mismatch; refusing extraction")


def extract_archive(archive: Path, destination: Path, directory_name: str) -> Path:
    if directory_name in ("", ".", "..") or "/" in directory_name or "\\" in directory_name:
        raise ValueError("Extraction root must be a directory basename")
    # Python's data filter rejects traversal and links escaping the extraction
    # root while preserving CEF's legitimate in-bundle framework symlinks.
    with tempfile.TemporaryDirectory(prefix=".extract-", dir=destination) as temporary:
        staging = Path(temporary)
        with tarfile.open(archive, "r:bz2") as contents:
            contents.extractall(staging, filter="data")
        entries = list(staging.iterdir())
        expected = staging / directory_name
        if entries != [expected] or not (expected / "include" / "cef_version.h").is_file():
            raise ValueError("CEF archive does not contain the expected distribution root")
        target = destination / directory_name
        if target.exists():
            raise ValueError(f"Extraction target already exists: {target}")
        expected.rename(target)
    return target


def fetch(selected_platform: str, destination: Path) -> Path:
    pin = json.loads(PIN_PATH.read_text(encoding="utf-8"))
    validate_pin(pin)
    artifact = pin["artifacts"][selected_platform]
    filename = artifact["filename"]
    directory_name = filename.removesuffix(".tar.bz2")
    destination.mkdir(parents=True, exist_ok=True)
    target = destination / directory_name
    receipt_path = target / ".kunlun-cef-pin.json"
    receipt = {
        "cefVersion": pin["cefVersion"],
        "platform": selected_platform,
        "sha256": artifact["sha256"],
    }
    if target.exists():
        if (
            receipt_path.is_file()
            and json.loads(receipt_path.read_text()) == receipt
            and (target / "include" / "cef_version.h").is_file()
        ):
            return target
        raise ValueError(f"Existing CEF directory has no matching pin receipt: {target}")

    archive = destination / filename
    if not archive.exists():
        url = DOWNLOAD_ORIGIN + urllib.parse.quote(filename, safe="")
        print(f"Downloading {filename} ({artifact['size']} bytes)", flush=True)
        # A temporary download is never treated as an accepted archive.
        temporary_path = None
        try:
            with tempfile.NamedTemporaryFile(
                prefix=".download-", dir=destination, delete=False
            ) as temporary:
                temporary_path = Path(temporary.name)
                with urllib.request.urlopen(url, timeout=60) as response:
                    copied = 0
                    while chunk := response.read(1024 * 1024):
                        copied += len(chunk)
                        if copied > artifact["size"]:
                            raise ValueError("Download exceeded the pinned archive size")
                        temporary.write(chunk)
                temporary.flush()
                os.fsync(temporary.fileno())
            verify_archive(temporary_path, artifact["sha256"], artifact["size"])
            # Publish only a complete verified file, atomically on this volume.
            temporary_path.replace(archive)
        finally:
            if temporary_path:
                temporary_path.unlink(missing_ok=True)
    verify_archive(archive, artifact["sha256"], artifact["size"])
    target = extract_archive(archive, destination, directory_name)
    receipt_path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return target


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("macosarm64", "macosx64"))
    parser.add_argument("--destination", type=Path, default=ROOT / ".deps" / "cef")
    arguments = parser.parse_args()
    try:
        selected_platform = arguments.platform or native_platform()
        destination = arguments.destination.resolve()
        # Downloads and extraction belong in this attached workspace, not an
        # arbitrary user directory. External SDKs may instead be read via CEF_ROOT.
        if not destination.is_relative_to(ROOT):
            raise ValueError("The download destination must be inside the repository")
        target = fetch(selected_platform, destination)
        print(f"CEF_ROOT={target}", flush=True)
    except (OSError, ValueError, KeyError, tarfile.TarError) as error:
        parser.exit(1, f"CEF fetch failed: {error}\n")


if __name__ == "__main__":
    main()
