#!/usr/bin/env python3
"""Test the packaged preview, not a substitute browser or mock renderer."""

import argparse
import json
import os
import signal
import subprocess
from pathlib import Path


def check_bundle(executable: Path) -> None:
    contents = executable.parent.parent
    framework = contents / "Frameworks" / "Chromium Embedded Framework.framework"
    links = {
        framework / "Chromium Embedded Framework": "Versions/A/Chromium Embedded Framework",
        framework / "Libraries": "Versions/A/Libraries",
        framework / "Resources": "Versions/A/Resources",
        framework / "Versions" / "Current": "A",
    }
    for link, target in links.items():
        if not link.is_symlink() or link.readlink().as_posix() != target or not link.exists():
            raise RuntimeError(f"Invalid CEF framework link: {link}")
    for path in framework.rglob("*"):
        if path.is_symlink() and not path.exists():
            raise RuntimeError(f"Broken nested CEF framework link: {path}")
    if not (framework / "Libraries" / "libcef_sandbox.dylib").is_file():
        raise RuntimeError("Missing bundled macOS CEF sandbox library")
    for suffix in ("", " (Alerts)", " (GPU)", " (Plugin)", " (Renderer)"):
        name = f"Kunlun Desktop Helper{suffix}"
        helper = contents / "Frameworks" / f"{name}.app" / "Contents" / "MacOS" / name
        if not helper.is_file():
            raise RuntimeError(f"Missing CEF helper: {name}")
    for notice in ("LICENSE.txt", "CREDITS.html", "LICENSE", "THIRD_PARTY_NOTICES.md"):
        if not (contents / "Resources" / "licenses" / notice).is_file():
            raise RuntimeError(f"Missing bundled license notice: {notice}")
    if not (contents / "Resources" / "pin.json").is_file():
        raise RuntimeError("Missing bundled CEF pin metadata")
    pin = json.loads((contents / "Resources" / "pin.json").read_text(encoding="utf-8"))
    source_pin = Path(__file__).resolve().parents[1] / "cef" / "pin.json"
    if pin != json.loads(source_pin.read_text(encoding="utf-8")):
        raise RuntimeError("Bundled CEF pin metadata does not match the source pin")


def run(executable: Path, argument: str, timeout: int) -> tuple[int, str]:
    process = subprocess.Popen(
        [str(executable), argument],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        start_new_session=True,
    )
    try:
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        # Only our test process group, including its CEF children.
        os.killpg(process.pid, signal.SIGKILL)
        process.communicate()
        raise RuntimeError(f"Native launch timed out: {argument}") from None
    return process.returncode, output


def check(executable: Path) -> None:
    check_bundle(executable)
    forbidden = (
        "--no-sandbox",
        "--disable-gpu-sandbox",
        "--single-process",
        "--disable-web-security",
        "--remote-debugging-port=9222",
        "--browser-subprocess-path=/tmp/helper",
        "--url=https://example.invalid/",
    )
    for argument in forbidden:
        code, output = run(executable, argument, 5)
        if code != 2 or "Chromium overrides are forbidden" not in output:
            raise RuntimeError(f"Forbidden argument was not rejected: {argument}\n{output}")
    print(f"Native launcher rejected {len(forbidden)} forbidden overrides", flush=True)

    code, output = run(executable, "--smoke-test", 45)
    print(output, end="", flush=True)
    if code != 0 or "Kunlun native smoke PASS:" not in output:
        raise RuntimeError(f"Packaged native smoke failed with exit code {code}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    arguments = parser.parse_args()
    try:
        check(arguments.executable.resolve(strict=True))
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f"Native check failed: {error}\n")


if __name__ == "__main__":
    main()
