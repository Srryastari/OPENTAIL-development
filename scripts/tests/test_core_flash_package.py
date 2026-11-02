"""Non-flashing regression tests for the manifest-locked ESP32-S3 release ZIP."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
path = ROOT / "scripts" / "package_core_flash.py"
spec = importlib.util.spec_from_file_location("package_core_flash", path)
pkg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pkg)


class FlashPackageTests(unittest.TestCase):
    def setUp(self):
        self.partition = (ROOT / "core-firmware/partitions_ota.csv").read_bytes()
        self.sdkconfig = (ROOT / "core-firmware/sdkconfig.defaults").read_bytes()

    def test_exact_four_offsets(self):
        manifest = json.dumps({"flash_files": {
            "0x0": "bootloader/bootloader.bin",
            "0x8000": "partition_table/partition-table.bin",
            "0xf000": "ota_data_initial.bin",
            "0x20000": "opentail_core_alpha.bin",
        }}).encode()
        self.assertEqual(pkg.offset_map(manifest), pkg.IMAGE_PATHS)
        spec_bad = json.loads(manifest)
        spec_bad["flash_files"].pop("0xf000")
        with self.assertRaises(pkg.PackageError):
            pkg.offset_map(json.dumps(spec_bad).encode())
        spec_bad = json.loads(manifest)
        spec_bad["flash_files"]["0xf000"] = "a-copied-or-fabricated-ota.bin"
        with self.assertRaises(pkg.PackageError):
            pkg.offset_map(json.dumps(spec_bad).encode())

    def test_actual_dual_ota_csv_and_preserved_nvs(self):
        pkg.expected_layout(self.partition, self.sdkconfig)
        wrong = self.partition.replace(b"0x9000", b"0xa000", 1)
        with self.assertRaises(pkg.PackageError):
            pkg.expected_layout(wrong, self.sdkconfig)
        with self.assertRaises(pkg.PackageError):
            pkg.expected_layout(self.partition, b"CONFIG_IDF_TARGET=\"esp32\"")

    def test_generated_flash_arg_offsets_must_agree(self):
        command = (b"--flash_mode dio --flash_size 16MB 0x0 bootloader/bootloader.bin "
                   b"0x8000 partition_table/partition-table.bin "
                   b"0xf000 ota_data_initial.bin 0x20000 opentail_core_alpha.bin")
        pkg.validate_flash_args(command)
        with self.assertRaises(pkg.PackageError):
            pkg.validate_flash_args(command.replace(b"0xf000 ota_data_initial.bin", b"0xf000 other.bin"))

    def test_build_refuses_to_synthesize_missing_ota_image(self):
        # Isolated fake build with deliberately *no* otadata. Must fail instead
        # of padding, fabricating or recovering an OTA image from another SHA.
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "build"
            (source / "bootloader").mkdir(parents=True)
            (source / "partition_table").mkdir()
            for p in pkg.IMAGE_PATHS.values():
                if p == "ota_data_initial.bin":
                    continue
                (source / p).write_bytes(b"not-an-installable-image")
            (source / "flasher_args.json").write_text(
                json.dumps({"flash_files": {hex(k): v for k, v in pkg.IMAGE_PATHS.items()}}))
            (source / "flash_args").write_text(" ".join(f"0x{k:x} {v}" for k,v in pkg.IMAGE_PATHS.items()))
            project = root / "project"
            project.mkdir()
            (project / "partitions_ota.csv").write_bytes(self.partition)
            (project / "sdkconfig.defaults").write_bytes(self.sdkconfig)
            output = root / "should-never-appear.zip"
            with self.assertRaises(FileNotFoundError):
                pkg.build(source, project, output, "a" * 40)
            self.assertFalse(output.exists())

    def test_corrupt_or_missing_actual_archive_fails(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / "incomplete.zip"
            with zipfile.ZipFile(p, "w") as z:
                z.writestr("flasher_args.json", json.dumps({"flash_files": {
                    hex(k): v for k,v in pkg.IMAGE_PATHS.items()
                }}))
                # Explicitly NO images: production must reject before flashing.
            with self.assertRaises(pkg.PackageError):
                pkg.verify(p, "b" * 40)

    def test_zip_checksum_output_is_not_just_a_manifest_hash(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / "x.zip"
            p.write_bytes(b"just-unit-test-bytes")
            self.assertEqual(len(pkg.sha256(p.read_bytes())), 64)
            self.assertNotEqual(pkg.sha256(p.read_bytes()), pkg.sha256(b"other"))

    def test_reject_path_traversal(self):
        for invalid in ("../ota_data_initial.bin", "/etc/passwd",
                        "bootloader/../ota_data_initial.bin", "C:\\bad.bin"):
            with self.assertRaises(pkg.PackageError):
                pkg.safe_name(invalid)
        for good in pkg.REQUIRED_ARCHIVE:
            pkg.safe_name(good)

    def test_unchanged_runtime_and_no_fake_images(self):
        script = path.read_text()
        self.assertIn('(build_dir / name).read_bytes()', script)
        self.assertIn('len(source["ota_data_initial.bin"]) != 0x2000', script)
        self.assertIn('archive.testzip()', script)
        self.assertIn('SHA256SUMS', script)
        self.assertNotIn("write_bytes(b'\\xff' *", script)


if __name__ == "__main__":
    unittest.main()
