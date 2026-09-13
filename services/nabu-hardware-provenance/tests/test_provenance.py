#!/usr/bin/python3
"""Black-box tests for the native C++ provenance inventory."""
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

BINARY = Path(os.environ.get("NABU_PROVENANCE_BINARY", "./nabu-hardware-provenance"))
FDT_MAGIC = 0xD00DFEED
DT_TABLE_MAGIC = 0xD7B7AB1E

def padded(value):
    return value + b"\0" * ((-len(value)) % 4)

def fdt_blob(compatible):
    strings = b"compatible\0"
    structure = b"".join((struct.pack(">I", 1), padded(b"\0"), struct.pack(">III", 3, len(compatible), 0), padded(compatible), struct.pack(">I", 2), struct.pack(">I", 9)))
    reserve = b"\0" * 16
    structure_offset = 40 + len(reserve)
    strings_offset = structure_offset + len(structure)
    total = strings_offset + len(strings)
    return struct.pack(">10I", FDT_MAGIC, total, structure_offset, strings_offset, 40, 17, 16, 0, len(strings), len(structure)) + reserve + structure + strings

def dtbo_image(variant=b"a", count=13):
    overlays = [fdt_blob(b"xiaomi,nabu\0" + (b"36-02-0b" if i % 2 else b"42-02-0a") + b"\0variant-" + variant + bytes([i])) for i in range(count)]
    entries_offset, payload_offset = 32, 32 + count * 32
    entries, payload = bytearray(), bytearray()
    for index, overlay in enumerate(overlays):
        entries.extend(struct.pack(">8I", len(overlay), payload_offset + len(payload), index, 1, 0, 0, 0, 0))
        payload.extend(overlay)
    total = payload_offset + len(payload)
    return struct.pack(">8I", DT_TABLE_MAGIC, total, 32, 32, count, entries_offset, 4096, 0) + entries + payload

class Fixture:
    def __init__(self, root):
        self.root = Path(root); self.sys = self.root / "sys"; self.dev = self.root / "dev"; self.firmware = self.root / "firmware"; self.etc = self.root / "etc"; self.proc = self.root / "proc"; self.output = self.root / "run/report.json"
        base = self.sys / "firmware/devicetree/base"; base.mkdir(parents=True); (base / "compatible").write_bytes(b"xiaomi,nabu\0"); (base / "model").write_bytes(b"Xiaomi Pad 5\0")
        self.proc.mkdir(); (self.proc / "cmdline").write_text("root=PARTLABEL=linux\n")
        (self.dev / "disk/by-partlabel").mkdir(parents=True); (self.dev / "nodes").mkdir()
        for relative in ("qcom/sm8150/xiaomi/nabu/adsp.mbn", "qcom/sm8150/xiaomi/nabu/cdsp.mbn", "qcom/sm8150/xiaomi/nabu/modem.mbn", "qcom/sm8150/xiaomi/nabu/slpi_nb.mbn", "qcom/sm8150/xiaomi/nabu/wlanmdsp.mbn", "qcom/sm8150/xiaomi/nabu/hexagonfs/sensors/sns_reg.conf", "ath10k/WCN3990/hw1.0/board-2.bin", "ath10k/WCN3990/hw1.0/firmware-5.bin", "qca/crbtfw32.tlv", "qca/crnv32.bin"):
            path = self.firmware / relative; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(b"packaged")

    def partition(self, label, content):
        node = self.dev / "nodes" / label; node.write_bytes(content); (self.dev / "disk/by-partlabel" / label).symlink_to(node); return node

    def run(self, strict=False):
        command = [BINARY, "--sys-root", self.sys, "--dev-root", self.dev, "--firmware-root", self.firmware, "--etc-root", self.etc, "--proc-cmdline", self.proc / "cmdline", "--output", self.output, "--allow-regular-fixtures"]
        if strict: command.append("--strict")
        result = subprocess.run(command, text=True, capture_output=True)
        return result, json.loads(self.output.read_text())

class ProvenanceTests(unittest.TestCase):
    def test_mirrored_thirteen_entry_inventory(self):
        with tempfile.TemporaryDirectory() as root:
            f = Fixture(root); image = dtbo_image(); f.partition("dtbo_a", image); f.partition("dtbo_b", image); result, report = f.run(True)
            self.assertEqual(result.returncode, 0, result.stderr); decision = report["dtboReference"]["slotDecision"]
            self.assertIsNone(decision["activeAndroidSlot"]); self.assertEqual(decision["referenceSlot"], "a"); self.assertEqual(decision["confidence"], "content-equivalent"); self.assertEqual(report["dtboReference"]["selectedInventory"]["entryCount"], 13); self.assertFalse(report["safetyContract"]["overlaysApplied"])

    def test_divergent_pair_without_slot_fails_closed(self):
        with tempfile.TemporaryDirectory() as root:
            f = Fixture(root); f.partition("dtbo_a", dtbo_image(b"a")); f.partition("dtbo_b", dtbo_image(b"b")); result, report = f.run(True)
            self.assertEqual(result.returncode, 1); self.assertIn("dtbo-slot-ambiguous", report["strictGateFailures"]); self.assertIsNone(report["dtboReference"]["selectedInventory"])

    def test_boot_slot_selects_reference_only(self):
        with tempfile.TemporaryDirectory() as root:
            f = Fixture(root); f.partition("dtbo_a", dtbo_image(b"a")); f.partition("dtbo_b", dtbo_image(b"b")); (f.proc / "cmdline").write_text("root=PARTLABEL=linux androidboot.slot_suffix=_b\n")
            result, report = f.run(True); self.assertEqual(result.returncode, 0, result.stderr); self.assertEqual(report["dtboReference"]["slotDecision"]["referenceSlot"], "b"); self.assertFalse(report["dtboReference"]["selectedInventory"]["applicationCandidateSelected"])

    def test_sensitive_partitions_are_metadata_only(self):
        with tempfile.TemporaryDirectory() as root:
            f = Fixture(root); image = dtbo_image(); f.partition("dtbo_a", image); f.partition("dtbo_b", image)
            secrets = [b"PRIVATE-MODEM-NV-IMEI", b"PRIVATE-WIFI-MAC", b"PRIVATE-PERSIST-CALIBRATION"]
            for label, secret in zip(("modemst1", "fsg", "persist"), secrets): f.partition(label, secret)
            result, report = f.run(); serialized = json.dumps(report)
            self.assertEqual(result.returncode, 0); [self.assertNotIn(secret.decode(), serialized) for secret in secrets]
            self.assertFalse(report["safetyContract"]["modemNvRead"]); self.assertFalse(report["safetyContract"]["radioAddressesRead"]); self.assertFalse(report["safetyContract"]["cameraEepromPayloadRead"])

    def test_invalid_and_overlapping_dtbo_are_rejected(self):
        with tempfile.TemporaryDirectory() as root:
            f = Fixture(root); f.partition("dtbo_a", b"not-a-dtbo"); result, report = f.run(True)
            self.assertEqual(result.returncode, 1); self.assertIn("dtbo_a-invalid", report["strictGateFailures"])
        with tempfile.TemporaryDirectory() as root:
            f = Fixture(root); image = bytearray(dtbo_image(count=2)); first = struct.unpack_from(">I", image, 36)[0]; struct.pack_into(">I", image, 68, first); f.partition("dtbo_a", image); result, report = f.run(True)
            self.assertEqual(result.returncode, 1); self.assertIn("overlaps another payload", report["dtboReference"]["partitions"]["a"]["error"])

    def test_binary_is_native_elf(self):
        self.assertEqual(BINARY.read_bytes()[:4], b"\x7fELF")

if __name__ == "__main__":
    unittest.main()
