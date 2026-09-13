#!/usr/bin/python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import argparse
import importlib.machinery
import importlib.util
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock


PROGRAM = Path(__file__).resolve().parents[1] / "nabu-hardware-provenance"
LOADER = importlib.machinery.SourceFileLoader("nabu_hardware_provenance", str(PROGRAM))
SPEC = importlib.util.spec_from_loader(LOADER.name, LOADER)
assert SPEC is not None
PROVENANCE = importlib.util.module_from_spec(SPEC)
LOADER.exec_module(PROVENANCE)


def padded(value: bytes) -> bytes:
    return value + b"\0" * ((-len(value)) % 4)


def fdt_blob(compatible: bytes) -> bytes:
    strings = b"compatible\0"
    structure = b"".join(
        (
            struct.pack(">I", PROVENANCE.FDT_BEGIN_NODE),
            padded(b"\0"),
            struct.pack(">III", PROVENANCE.FDT_PROP, len(compatible), 0),
            padded(compatible),
            struct.pack(">I", PROVENANCE.FDT_END_NODE),
            struct.pack(">I", PROVENANCE.FDT_END),
        )
    )
    reserve = b"\0" * 16
    structure_offset = 40 + len(reserve)
    strings_offset = structure_offset + len(structure)
    total_size = strings_offset + len(strings)
    header = struct.pack(
        ">10I",
        PROVENANCE.FDT_MAGIC,
        total_size,
        structure_offset,
        strings_offset,
        40,
        17,
        16,
        0,
        len(strings),
        len(structure),
    )
    return header + reserve + structure + strings


def dtbo_image(variant: bytes = b"a", entry_count: int = 13) -> bytes:
    overlays = []
    for index in range(entry_count):
        panel = b"36-02-0b" if index % 2 else b"42-02-0a"
        overlays.append(
            fdt_blob(b"xiaomi,nabu\0" + panel + b"\0variant-" + variant + bytes([index]))
        )
    entries_offset = 32
    payload_offset = entries_offset + entry_count * 32
    entries = bytearray()
    payload = bytearray()
    for index, overlay in enumerate(overlays):
        entries.extend(
            struct.pack(
                ">8I",
                len(overlay),
                payload_offset + len(payload),
                index,
                1,
                0,
                0,
                0,
                0,
            )
        )
        payload.extend(overlay)
    total_size = payload_offset + len(payload)
    header = struct.pack(
        ">8I",
        PROVENANCE.DT_TABLE_MAGIC,
        total_size,
        32,
        32,
        entry_count,
        entries_offset,
        4096,
        0,
    )
    return header + bytes(entries) + bytes(payload)


class Fixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.sys = root / "sys"
        self.dev = root / "dev"
        self.firmware = root / "firmware"
        self.etc = root / "etc"
        self.proc = root / "proc"
        self.output = root / "run" / "provenance.json"
        compatible = self.sys / "firmware" / "devicetree" / "base" / "compatible"
        compatible.parent.mkdir(parents=True)
        compatible.write_bytes(b"xiaomi,nabu\0")
        compatible.with_name("model").write_bytes(b"Xiaomi Pad 5\0")
        self.proc.mkdir()
        (self.proc / "cmdline").write_text("root=PARTLABEL=linux\n", encoding="utf-8")
        (self.dev / "disk" / "by-partlabel").mkdir(parents=True)
        (self.dev / "nodes").mkdir()
        for _, relative in PROVENANCE.FIRMWARE_FILES:
            destination = self.firmware / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(b"packaged-firmware")

    def partition(self, label: str, content: bytes) -> Path:
        node = self.dev / "nodes" / label
        node.write_bytes(content)
        link = self.dev / "disk" / "by-partlabel" / label
        link.symlink_to(node)
        return node

    def arguments(self) -> argparse.Namespace:
        return argparse.Namespace(
            sys_root=self.sys,
            dev_root=self.dev,
            firmware_root=self.firmware,
            etc_root=self.etc,
            proc_cmdline=self.proc / "cmdline",
            output=self.output,
            stdout=False,
            strict=False,
            allow_regular_fixtures=True,
        )


class ProvenanceTests(unittest.TestCase):
    def test_mirrored_thirteen_entry_inventory_does_not_claim_active_slot(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = Fixture(Path(temporary))
            image = dtbo_image()
            fixture.partition("dtbo_a", image)
            fixture.partition("dtbo_b", image)

            report, failures = PROVENANCE.build_report(fixture.arguments())

            self.assertEqual(failures, [])
            decision = report["dtboReference"]["slotDecision"]
            self.assertIsNone(decision["activeAndroidSlot"])
            self.assertEqual(decision["referenceSlot"], "a")
            self.assertEqual(decision["confidence"], "content-equivalent")
            inventory = report["dtboReference"]["selectedInventory"]
            self.assertEqual(inventory["entryCount"], 13)
            self.assertEqual(len(inventory["entries"]), 13)
            self.assertFalse(inventory["applicationCandidateSelected"])

    def test_divergent_pair_without_boot_slot_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = Fixture(Path(temporary))
            fixture.partition("dtbo_a", dtbo_image(b"a"))
            fixture.partition("dtbo_b", dtbo_image(b"b"))

            report, failures = PROVENANCE.build_report(fixture.arguments())

            self.assertIn("dtbo-slot-ambiguous", failures)
            decision = report["dtboReference"]["slotDecision"]
            self.assertIsNone(decision["activeAndroidSlot"])
            self.assertIsNone(decision["referenceSlot"])
            self.assertEqual(decision["source"], "ambiguous")
            self.assertIsNone(report["dtboReference"]["selectedInventory"])

    def test_kernel_slot_selects_only_reference_and_never_applies_overlay(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = Fixture(Path(temporary))
            fixture.partition("dtbo_a", dtbo_image(b"a"))
            fixture.partition("dtbo_b", dtbo_image(b"b"))
            (fixture.proc / "cmdline").write_text(
                "root=PARTLABEL=linux androidboot.slot_suffix=_b\n", encoding="utf-8"
            )

            report, failures = PROVENANCE.build_report(fixture.arguments())

            self.assertEqual(failures, [])
            decision = report["dtboReference"]["slotDecision"]
            self.assertEqual(decision["activeAndroidSlot"], "b")
            self.assertEqual(decision["referenceSlot"], "b")
            self.assertEqual(decision["confidence"], "boot-chain")
            self.assertFalse(report["safetyContract"]["overlaysApplied"])

    def test_sensitive_payloads_and_addresses_are_never_read_or_exported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = Fixture(Path(temporary))
            image = dtbo_image()
            dtbo_a = fixture.partition("dtbo_a", image)
            dtbo_b = fixture.partition("dtbo_b", image)
            secrets = {
                "modemst1": b"PRIVATE-MODEM-NV-IMEI",
                "fsg": b"PRIVATE-WIFI-CALIBRATION-MAC",
                "persist": b"PRIVATE-PERSIST-CALIBRATION",
            }
            secret_paths = {
                fixture.partition(label, content) for label, content in secrets.items()
            }
            nvmem = (
                fixture.sys
                / "bus"
                / "nvmem"
                / "devices"
                / "nabu-rear-camera-calibration0"
                / "nvmem"
            )
            nvmem.parent.mkdir(parents=True)
            nvmem.write_bytes(b"PRIVATE-CAMERA-EEPROM")
            wifi = fixture.sys / "class" / "net" / "wlan0"
            (wifi / "wireless").mkdir(parents=True)
            (wifi / "addr_assign_type").write_text("3\n", encoding="utf-8")
            (wifi / "address").write_text("00:11:22:33:44:55\n", encoding="utf-8")

            real_open = os.open
            opened: list[Path] = []
            real_read_bytes = PROVENANCE.read_bytes
            byte_reads: list[Path] = []

            def recording_open(path: os.PathLike[str] | str, flags: int, *args: object) -> int:
                opened.append(Path(path).resolve())
                return real_open(path, flags, *args)

            def recording_read_bytes(path: Path, maximum: int) -> bytes:
                byte_reads.append(path.resolve())
                return real_read_bytes(path, maximum)

            with (
                mock.patch.object(PROVENANCE.os, "open", side_effect=recording_open),
                mock.patch.object(
                    PROVENANCE, "read_bytes", side_effect=recording_read_bytes
                ),
            ):
                report, _ = PROVENANCE.build_report(fixture.arguments())

            self.assertEqual(set(opened), {dtbo_a.resolve(), dtbo_b.resolve()})
            self.assertTrue(secret_paths.isdisjoint(opened))
            self.assertNotIn(nvmem.resolve(), opened)
            self.assertTrue(secret_paths.isdisjoint(byte_reads))
            self.assertNotIn(nvmem.resolve(), byte_reads)
            self.assertNotIn((wifi / "address").resolve(), byte_reads)
            serialized = json.dumps(report, sort_keys=True)
            for value in secrets.values():
                self.assertNotIn(value.decode(), serialized)
            self.assertNotIn("PRIVATE-CAMERA-EEPROM", serialized)
            self.assertNotIn("00:11:22:33:44:55", serialized)
            self.assertTrue(report["safetyContract"]["modemNvRead"] is False)
            self.assertTrue(report["safetyContract"]["radioAddressesRead"] is False)
            self.assertTrue(report["safetyContract"]["cameraEepromPayloadRead"] is False)

    def test_invalid_dtbo_is_bounded_and_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = Fixture(Path(temporary))
            fixture.partition("dtbo_a", b"not-a-dtbo")

            report, failures = PROVENANCE.build_report(fixture.arguments())

            self.assertIn("dtbo_a-invalid", failures)
            self.assertFalse(report["dtboReference"]["partitions"]["a"]["valid"])
            self.assertIsNone(report["dtboReference"]["slotDecision"]["referenceSlot"])

    def test_overlapping_dtbo_payloads_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = Fixture(Path(temporary))
            image = bytearray(dtbo_image(entry_count=2))
            first_payload_offset = struct.unpack_from(">I", image, 36)[0]
            struct.pack_into(">I", image, 68, first_payload_offset)
            fixture.partition("dtbo_a", bytes(image))

            report, failures = PROVENANCE.build_report(fixture.arguments())

            self.assertIn("dtbo_a-invalid", failures)
            error = report["dtboReference"]["partitions"]["a"]["error"]
            self.assertIn("overlaps another payload", error)


if __name__ == "__main__":
    unittest.main()
