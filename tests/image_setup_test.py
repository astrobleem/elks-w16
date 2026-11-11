#!/usr/bin/env python3
"""Check image-builder isolation with fake cross tools, not an emulator test."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class ImageSetup(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.w16 = self.root / "w16"
        self.elks = self.root / "elks"
        self.mw = self.root / "microwindows"
        self.bin = self.root / "bin"
        self.bin.mkdir()
        (self.w16 / "tests").mkdir(parents=True)
        shutil.copy(Path(__file__).with_name("mkimage.sh"), self.w16 / "tests/mkimage.sh")
        for path in ("src/w16", "test/hello16/HELLO16.EXE", "test/dlgtest/DLGTEST.EXE",
                     "test/mbtest/MBTEST.EXE"):
            p = self.w16 / path
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("fixture")
        for script in ("mkbad.py", "mkbadres.py"):
            (self.w16 / "tests" / script).write_text(
                "import pathlib, sys\npathlib.Path(sys.argv[2]).mkdir()\n")
        for path in ("src/bin/nano-x", "src/lib/libnano-X.a", "vidreset"):
            p = self.mw / path
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("fixture")
        (self.elks / "image").mkdir(parents=True)
        (self.elks / "target").mkdir()
        (self.elks / "image/Config").write_text("keep config\n")
        (self.elks / "target/keep").write_text("keep staging\n")
        (self.elks / ".config").write_text("CONFIG_TIME_TZ=UTC\n")
        (self.elks / "env.sh").write_text(":\n")
        stub = self.bin / "stub"
        stub.write_text('''#!/usr/bin/env python3
import os, pathlib, sys
name = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
if name == "ia16-elf-gcc":
    pathlib.Path(args[args.index("-o") + 1]).write_text("fixture binary")
elif name == "make":
    settings = dict(a.split("=", 1) for a in args if "=" in a)
    assert {"CONFIG", "DESTDIR", "TARGET_FILE"} <= settings.keys(), settings
    elks = pathlib.Path(os.environ["ELKS_ROOT"])
    assert pathlib.Path(settings["CONFIG"]) != elks / "image/Config"
    assert pathlib.Path(settings["DESTDIR"]) != elks / "target"
    assert pathlib.Path(settings["CONFIG"]).is_file()
    if os.environ.get("FAIL_IMAGE_MAKE"):
        sys.exit(19)
    pathlib.Path(settings["DESTDIR"]).mkdir()
    pathlib.Path(settings["TARGET_FILE"]).write_text("fixture image")
elif name == "mfs" and "extract" in args:
    pathlib.Path(args[-1]).write_text("# fixture\\n")
''')
        stub.chmod(0o755)
        for name in ("ia16-elf-gcc", "make", "mfs", "mfsck", "setboot"):
            (self.bin / name).symlink_to(stub)
        self.env = dict(os.environ, ELKS_ROOT=str(self.elks), MWDIR=str(self.mw),
                        PATH=str(self.bin) + os.pathsep + os.environ["PATH"])
        self.env.pop("W16_EXTRA", None)

    def run_image(self, name="out", *flags):
        return subprocess.run(["bash", str(self.w16 / "tests/mkimage.sh"), name, *flags],
                              cwd=self.root, env=self.env, capture_output=True, text=True)

    def assert_preserved(self):
        self.assertEqual((self.elks / "image/Config").read_text(), "keep config\n")
        self.assertEqual((self.elks / "target/keep").read_text(), "keep staging\n")
        self.assertEqual(list((self.elks / "target").iterdir()), [self.elks / "target/keep"])

    def test_config_and_staging_are_isolated(self):
        result = self.run_image("out", "repeat")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.root / "out.img").read_text(), "fixture image")
        self.assertEqual((self.root / "out-mbr.img").stat().st_size, 615 * 4 * 17 * 512)
        self.assert_preserved()

    def test_make_failure_keeps_existing_files(self):
        self.env["FAIL_IMAGE_MAKE"] = "1"
        self.assertNotEqual(self.run_image().returncode, 0)
        self.assertFalse((self.root / "out.img").exists())
        self.assert_preserved()

    def test_existing_output_is_refused(self):
        (self.root / "out.img").write_text("keep image")
        self.assertEqual(self.run_image().returncode, 2)
        self.assertEqual((self.root / "out.img").read_text(), "keep image")
        self.assert_preserved()

    def test_bad_flag_is_refused_before_build(self):
        self.assertEqual(self.run_image("out", "unknown").returncode, 2)
        self.assertFalse((self.root / "out.img").exists())
        self.assert_preserved()

    def test_whitespace_path_is_refused(self):
        self.assertEqual(self.run_image("test disk").returncode, 2)
        self.assertFalse((self.root / "test disk.img").exists())
        self.assert_preserved()


if __name__ == "__main__":
    unittest.main()
