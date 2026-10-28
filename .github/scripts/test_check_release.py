import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import check_release


class ReleaseGates(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for name, value in {
            "CMakeLists.txt": "project(mainuan-welcome VERSION 1.0 LANGUAGES CXX)\n",
            "debian/changelog": "mainuan-welcome (1.0) resolute; urgency=medium\n",
            "rootfs/usr/share/metainfo/org.mainuan.Welcome.metainfo.xml": '<component><releases><release version="1.0"/></releases></component>',
        }.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(value)

    def test_version_and_tag(self):
        self.assertEqual(check_release.metadata(self.root, "v1.0"), "1.0")
        for tag in ["v2.3.1.5", "v1.0;command", "v1.0\nversion=2.0", "main", "v1.0.0"]:
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                check_release.metadata(self.root, tag)

    def test_mismatched_metadata(self):
        (self.root / "debian/changelog").write_text("mainuan-welcome (2.3.1.5) resolute; urgency=medium\n")
        with self.assertRaises(ValueError):
            check_release.metadata(self.root)

    def test_patch_release_and_existing_tag(self):
        for name in ["CMakeLists.txt", "debian/changelog",
                     "rootfs/usr/share/metainfo/org.mainuan.Welcome.metainfo.xml"]:
            path = self.root / name
            path.write_text(path.read_text().replace("1.0", "1.0.1"))
        self.assertEqual(check_release.metadata(self.root, "v1.0.1"), "1.0.1")
        with self.assertRaises(ValueError):
            check_release.metadata(self.root, "v1.0")

    def test_personal_dependency(self):
        (self.root / "src").mkdir()
        (self.root / "src/personal.cpp").write_text('const char *reference = "/home/someone/reference";')
        with self.assertRaises(ValueError):
            check_release.metadata(self.root)

    def test_missing_and_skipped_tests_are_errors(self):
        report = self.root / "tests.xml"
        report.write_text('<testsuite><testcase name="tst_mainuan"/></testsuite>')
        with self.assertRaises(ValueError):
            check_release.tests(report)
        for status in ["", "<skipped/>", "<failure/>", "<error/>"]:
            cases = ''.join(f'<testcase name="{name}">{status}</testcase>' for name in check_release.EXPECTED_TESTS)
            report.write_text(f"<testsuite>{cases}</testsuite>")
            if status:
                with self.assertRaises(ValueError):
                    check_release.tests(report)
            else:
                self.assertEqual(check_release.tests(report), 6)

    def test_artificial_package_renaming_is_rejected(self):
        with patch("check_release.subprocess.check_output", side_effect=["mainuan-welcome\n", "2.3.1.5\n", "amd64\n"]):
            with self.assertRaises(ValueError):
                check_release.package(Path("mainuan-welcome_1.0_amd64.deb"), "1.0")

    def test_wrong_package_filename_is_rejected(self):
        with self.assertRaises(ValueError):
            check_release.package(Path("some-other-package_1.0_amd64.deb"), "1.0")


if __name__ == "__main__":
    unittest.main()
