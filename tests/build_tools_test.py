import copy
import hashlib
import io
import json
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.embed_assets import ASSET_FILES, render_header
from tools.fetch_cef import PIN_PATH, extract_archive, fetch, validate_pin, verify_archive


class BuildPlatformTests(unittest.TestCase):
    def test_windows_target_rejected_before_compiler_discovery(self):
        cmake = shutil.which("cmake")
        if cmake is None:
            self.skipTest("CMake is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run(
                [
                    cmake,
                    "-S", str(Path(__file__).resolve().parents[1]),
                    "-B", temporary,
                    "-DCMAKE_SYSTEM_NAME=Windows",
                ],
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
        output = result.stdout + result.stderr
        diagnostic = " ".join(output.split())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Windows is outside the current Kunlun Desktop scope", diagnostic)
        self.assertIn("Kunlun Runtime Windows port must be completed", diagnostic)
        self.assertNotIn("CXX compiler identification", output)


class CefPinTests(unittest.TestCase):
    def setUp(self):
        self.pin = json.loads(PIN_PATH.read_text(encoding="utf-8"))

    def test_committed_pin_is_valid(self):
        validate_pin(self.pin)

    def test_path_aliases_rejected(self):
        for filename in (
            "../../../escape.tar.bz2",
            "/absolute.tar.bz2",
            r"..\escape.tar.bz2",
            "%2e%2e%2fescape.tar.bz2",
            "unexpected.tar.bz2",
        ):
            pin = copy.deepcopy(self.pin)
            pin["artifacts"]["macosarm64"]["filename"] = filename
            with self.subTest(filename=filename), self.assertRaises(ValueError):
                validate_pin(pin)

    def test_schema_version_identity_and_hashes_required(self):
        for key, value in (
            ("schemaVersion", True),
            ("schemaVersion", 2),
            ("cefVersion", "../../escape"),
            ("cefCommit", "0" * 40),
            ("chromiumVersion", "1.2.3.4"),
        ):
            pin = copy.deepcopy(self.pin)
            pin[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                validate_pin(pin)
        for key, value in (("sha256", "bad"), ("size", -1), ("size", True)):
            pin = copy.deepcopy(self.pin)
            pin["artifacts"]["macosarm64"][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                validate_pin(pin)


class EmbeddedAssetsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        for name in ASSET_FILES:
            (self.root / name).write_text(f"fixture for {name}\n", encoding="utf-8")

    def tearDown(self):
        self.temporary.cleanup()

    def test_deterministic_and_fixed_asset_table(self):
        first = render_header(self.root)
        (self.root / "secret.txt").write_text("must not be bundled")
        self.assertEqual(first, render_header(self.root))
        for name, mime_type in ASSET_FILES.items():
            self.assertIn(f'"{name}", "{mime_type}"', first)
        self.assertNotIn("secret.txt", first)

    def test_missing_invalid_and_empty_assets_rejected(self):
        path = self.root / "app.js"
        path.unlink()
        with self.assertRaises(ValueError):
            render_header(self.root)
        path.write_bytes(b"\xff")
        with self.assertRaises(UnicodeDecodeError):
            render_header(self.root)
        path.write_bytes(b"")
        with self.assertRaises(ValueError):
            render_header(self.root)

    def test_symlink_asset_rejected(self):
        path = self.root / "app.js"
        path.unlink()
        try:
            path.symlink_to(self.root / "bridge.js")
        except OSError:
            self.skipTest("Symlinks are unavailable on this runner")
        with self.assertRaises(ValueError):
            render_header(self.root)


class CefArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.archive = self.root / "cef.tar.bz2"

    def tearDown(self):
        self.temporary.cleanup()

    def write_archive(self, entries):
        with tarfile.open(self.archive, "w:bz2") as archive:
            for name, content in entries:
                info = tarfile.TarInfo(name)
                info.size = len(content)
                archive.addfile(info, io.BytesIO(content))

    def test_hash_and_size_checked_before_extraction(self):
        self.archive.write_bytes(b"test archive")
        digest = hashlib.sha256(b"test archive").hexdigest()
        verify_archive(self.archive, digest, 12)
        with self.assertRaisesRegex(ValueError, "SHA-256"):
            verify_archive(self.archive, "0" * 64, 12)
        with self.assertRaisesRegex(ValueError, "size mismatch"):
            verify_archive(self.archive, digest, 11)

    def test_expected_root_required(self):
        self.write_archive([("wrong/include/cef_version.h", b"test")])
        with self.assertRaisesRegex(ValueError, "expected distribution root"):
            extract_archive(self.archive, self.root, "cef")
        self.assertFalse((self.root / "cef").exists())

    def test_traversal_rejected(self):
        self.write_archive([("../escaped", b"test")])
        with self.assertRaises(tarfile.FilterError):
            extract_archive(self.archive, self.root, "cef")
        self.assertFalse((self.root / "escaped").exists())

    def test_escaping_symlink_rejected(self):
        with tarfile.open(self.archive, "w:bz2") as archive:
            info = tarfile.TarInfo("cef/escape")
            info.type = tarfile.SYMTYPE
            info.linkname = "../../escaped"
            archive.addfile(info)
        with self.assertRaises(tarfile.FilterError):
            extract_archive(self.archive, self.root, "cef")

    def test_extraction_root_cannot_escape_destination(self):
        for root in ("../escape", r"..\escape", ".", ""):
            with self.subTest(root=root), self.assertRaises(ValueError):
                extract_archive(self.archive, self.root, root)

    def test_valid_root_extracted_without_overwriting_existing(self):
        self.write_archive([("cef/include/cef_version.h", b"test")])
        target = extract_archive(self.archive, self.root, "cef")
        self.assertEqual((target / "include/cef_version.h").read_bytes(), b"test")
        with self.assertRaisesRegex(ValueError, "already exists"):
            extract_archive(self.archive, self.root, "cef")


class CefDownloadTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.destination = self.root / "downloads"
        self.pin = json.loads(PIN_PATH.read_text(encoding="utf-8"))
        self.filename = self.pin["artifacts"]["macosarm64"]["filename"]
        directory = self.filename.removesuffix(".tar.bz2")
        buffer = io.BytesIO()
        with tarfile.open(fileobj=buffer, mode="w:bz2") as archive:
            content = b"test SDK header"
            info = tarfile.TarInfo(f"{directory}/include/cef_version.h")
            info.size = len(content)
            archive.addfile(info, io.BytesIO(content))
        self.content = buffer.getvalue()
        for artifact in self.pin["artifacts"].values():
            artifact["sha256"] = hashlib.sha256(self.content).hexdigest()
            artifact["size"] = len(self.content)
        self.pin_path = self.root / "pin.json"
        self.pin_path.write_text(json.dumps(self.pin), encoding="utf-8")

    def tearDown(self):
        self.temporary.cleanup()

    def test_verified_download_is_published_atomically_then_reused(self):
        published = []
        original_replace = Path.replace

        def publish(source, target):
            self.assertEqual(target, self.destination / self.filename)
            self.assertFalse(target.exists())
            artifact = self.pin["artifacts"]["macosarm64"]
            verify_archive(source, artifact["sha256"], artifact["size"])
            published.append(target)
            return original_replace(source, target)

        with (
            patch("tools.fetch_cef.PIN_PATH", self.pin_path),
            patch("tools.fetch_cef.urllib.request.urlopen", return_value=io.BytesIO(self.content)) as get,
            patch.object(Path, "replace", new=publish),
        ):
            target = fetch("macosarm64", self.destination)
            self.assertEqual(fetch("macosarm64", self.destination), target)
        self.assertEqual(len(published), 1)
        self.assertEqual(get.call_count, 1)
        self.assertTrue((target / ".kunlun-cef-pin.json").is_file())
        self.assertEqual(list(self.destination.glob(".download-*")), [])

    def test_bad_download_never_becomes_a_cached_archive(self):
        with (
            patch("tools.fetch_cef.PIN_PATH", self.pin_path),
            patch("tools.fetch_cef.urllib.request.urlopen", return_value=io.BytesIO(b"wrong data")),
        ):
            with self.assertRaises(ValueError):
                fetch("macosarm64", self.destination)
        self.assertFalse((self.destination / self.filename).exists())
        self.assertEqual(list(self.destination.glob(".download-*")), [])

    def test_interrupted_download_never_becomes_a_cached_archive(self):
        class Interrupted(io.BytesIO):
            def read(self, size=-1):
                if self.tell():
                    raise OSError("network interrupted")
                return super().read(size)

        with (
            patch("tools.fetch_cef.PIN_PATH", self.pin_path),
            patch("tools.fetch_cef.urllib.request.urlopen", return_value=Interrupted(self.content)),
        ):
            with self.assertRaisesRegex(OSError, "network interrupted"):
                fetch("macosarm64", self.destination)
        self.assertFalse((self.destination / self.filename).exists())
        self.assertEqual(list(self.destination.glob(".download-*")), [])


if __name__ == "__main__":
    unittest.main()
