import json
import hashlib
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/release"))
import distribution as release


class DistributionTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.stage = Path(self.temp.name) / "stage"
        self.stage.mkdir()
        self.out = self.stage.parent / "dist"
        self.info = release.identity("1.2.3", "stable", "1" * 40)
        (self.stage / "build-info.json").write_text(json.dumps(self.info))
        (self.stage / "acheron.exe").write_bytes(b"synthetic")

    def test_channels(self):
        release.identity("0.4.0-preview.2", "preview", "f" * 40)
        for version, channel in [("01.2.3", "stable"), ("0.4.0", "preview"), ("0.4.0-preview.1", "stable"),
                                 ("0.4.0-preview.01", "preview"), ("1.2.3", "development"), ("../1.2.3", "stable")]:
            with self.subTest(version=version, channel=channel), self.assertRaises(ValueError):
                release.identity(version, channel, "1" * 40)
        with self.assertRaises(ValueError):
            release.identity("0.4.0", "stable", "HEAD")

    def test_roundtrip(self):
        release.create(self.stage, self.out, self.info)
        release.verify(self.out, self.info)
        package, = json.loads((self.out / "manifest.json").read_text())["packages"]
        data = (self.out / package["filename"]).read_bytes()
        self.assertEqual(package["size"], len(data))
        self.assertEqual(package["sha256"], hashlib.sha256(data).hexdigest())

    def test_wrong_binary_or_name(self):
        with self.assertRaises(ValueError):
            release.create(self.stage, self.out, dict(self.info, channel="preview"))
        with self.assertRaises(ValueError):
            release.manifest(self.out / "other.zip", self.info)

    def test_tampering(self):
        release.create(self.stage, self.out, self.info)
        manifest = self.out / "manifest.json"
        original = manifest.read_bytes()
        for field, value in [("schema_version", True), ("release_notes", ""), ("packages", [])]:
            data = json.loads(original)
            data[field] = value
            manifest.write_text(release.encode(data))
            with self.assertRaises(ValueError):
                release.verify(self.out, self.info)
        manifest.write_bytes(original)
        with (self.out / release.asset_name(self.info)).open("ab") as archive:
            archive.write(b"tampered")
        with self.assertRaises(ValueError):
            release.verify(self.out, self.info)


if __name__ == "__main__":
    unittest.main()
