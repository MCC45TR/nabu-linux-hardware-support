# Nabu hardware provenance

`nabu-hardware-provenance` creates
`/run/nabu-hardware/provenance.json`, a privacy-preserving inventory of the
hardware data sources available to Linux on Xiaomi Pad 5 (`nabu`).  The report
is intended for diagnostics and packaging gates; it is not a bootloader, an
Android partition importer, or a device-tree overlay loader.

## Safety contract

- Only `dtbo_a` and `dtbo_b` may be opened, and only with `O_RDONLY` and a
  64 MiB maximum read.  Their Android DT table and FDT structures are validated
  before metadata is published.
- DSP, modem, Bluetooth, persist and modem-NV partitions are never mounted,
  read, copied, hashed, parsed or extracted.  Only presence, size and device
  node access metadata are reported.
- Camera NVMEM payloads, Wi-Fi MAC addresses, Bluetooth addresses, modem
  identities and calibration bytes are never read or included in the report.
- Android DTBO entries are an inventory-only reference.  No overlay is applied
  and the tool never chooses an overlay application candidate.
- Packaged files below `/usr/lib/firmware` remain the Linux authority, so the
  report and normal Linux boot continue to work on Linux-only installations.

The system service adds independent enforcement layers.  A dedicated
`nabu-provenance` account receives Unix read permission only for DTBO A/B.  The
device cgroup allows both the Nabu SCSI/UFS `sd` LUN class and its dynamic
`blkext` partition class for reads only.  Capabilities are empty, the filesystem is protected,
networking is denied, and mount/reboot interfaces are outside the syscall
allow-list.  The broad device-class read gate is therefore narrowed by the
dedicated account's per-node permissions.

On Android-capable layouts, udev requests the service as soon as `dtbo_b` is
enumerated (after `dtbo_a` on Nabu).  The normal multi-user preset remains a
fallback, so Linux-only installations still inventory packaged firmware and
kernel NVMEM providers without requiring Android partitions.

## Slot decision

The report distinguishes an **active Android slot** from a **reference slot**.
An active slot is reported only when the boot chain supplies a valid
`androidboot.slot*` kernel command-line value.  If A and B are byte-identical,
slot activity remains unknown and A is used only as a content-equivalent
reference.  Divergent images without a boot-chain slot fail closed: neither is
selected.  A single image may be inventoried but is not called active.

## Usage

Generate the runtime report:

```sh
sudo systemctl start nabu-hardware-provenance.service
jq . /run/nabu-hardware/provenance.json
```

For release qualification, request a non-zero exit when a structure is invalid,
A/B selection is ambiguous, the device is not Nabu, or required packaged
firmware is missing:

```sh
sudo /usr/libexec/senemos-nabu/nabu-hardware-provenance --strict --stdout
```

Strict mode is deliberately not used by the boot service.  Linux-only systems
without Android partitions remain supported, while any partitions that are
present are still validated conservatively.

The installed service is a C++20/QtCore ELF binary. Python is confined to the
isolated black-box fixture tests:

```sh
NABU_PROVENANCE_BINARY=./nabu-hardware-provenance \
  python3 -m unittest discover -s services/nabu-hardware-provenance/tests -v
```
