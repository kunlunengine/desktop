import tempfile
import unittest
from pathlib import Path

from tools.check_native import check_bundle
from tools.fetch_cef import PIN_PATH


class NativeBundleTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.contents = Path(temporary.name) / "Kunlun Desktop.app" / "Contents"
        self.executable = self.contents / "MacOS" / "Kunlun Desktop"
        self.write(self.executable)
        self.framework = self.contents / "Frameworks" / "Chromium Embedded Framework.framework"
        version = self.framework / "Versions" / "A"
        self.write(version / "Chromium Embedded Framework")
        self.write(version / "Libraries" / "libcef_sandbox.dylib")
        self.write(version / "Resources" / "fixture.txt")
        links = {
            self.framework / "Chromium Embedded Framework": "Versions/A/Chromium Embedded Framework",
            self.framework / "Libraries": "Versions/A/Libraries",
            self.framework / "Resources": "Versions/A/Resources",
            self.framework / "Versions" / "Current": "A",
        }
        for link, target in links.items():
            try:
                link.symlink_to(target, target_is_directory=link.name != "Chromium Embedded Framework")
            except OSError:
                self.skipTest("Symlinks are unavailable on this runner")
        for suffix in ("", " (Alerts)", " (GPU)", " (Plugin)", " (Renderer)"):
            name = f"Kunlun Desktop Helper{suffix}"
            self.write(self.contents / "Frameworks" / f"{name}.app" / "Contents" / "MacOS" / name)
        for notice in ("LICENSE.txt", "CREDITS.html", "LICENSE", "THIRD_PARTY_NOTICES.md"):
            self.write(self.contents / "Resources" / "licenses" / notice)
        (self.contents / "Resources" / "pin.json").write_bytes(PIN_PATH.read_bytes())

    @staticmethod
    def write(path):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("fixture", encoding="utf-8")

    def test_complete_bundle_layout(self):
        check_bundle(self.executable)

    def test_broken_incremental_framework_link_rejected(self):
        broken = self.framework / "Versions" / "A" / "Libraries" / "Libraries"
        broken.symlink_to("Versions/A/Libraries", target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, "Broken nested"):
            check_bundle(self.executable)

    def test_missing_helper_rejected(self):
        name = "Kunlun Desktop Helper (Renderer)"
        helper = self.contents / "Frameworks" / f"{name}.app" / "Contents" / "MacOS" / name
        helper.unlink()
        with self.assertRaisesRegex(RuntimeError, "Missing CEF helper"):
            check_bundle(self.executable)

    def test_pin_mismatch_rejected(self):
        (self.contents / "Resources" / "pin.json").write_text("{}", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "does not match"):
            check_bundle(self.executable)


if __name__ == "__main__":
    unittest.main()
