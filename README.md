# Nabu Linux hardware support

Userspace hardware profiles and integration files for the Xiaomi Pad 5
(`nabu`). This repository complements
[`nabu-linux-kernel`](https://github.com/MCC45TR/nabu-linux-kernel) and is
consumed by
[`nabu-linux-builder`](https://github.com/MCC45TR/nabu-linux-builder).

## Current status

| Area | Status | Details |
| --- | --- | --- |
| Display rotation | Validated | Rotation is enabled unconditionally for Plasma and SDDM. |
| 60 Hz profile | Validated | Display and touch both operate at 60 Hz with the matching kernel. |
| 120 Hz fallback | Validated | Available through the matching kernel and Plasma display settings. |
| Ambient light | Experimental | Brightening reacts to light, but dimming and calibration are not yet reliable. |
| Internal audio | Experimental | Four channels are exposed, but routing, balance, and high-volume distortion remain unresolved. |
| PMIC RTC | Validated | Early boot clock synchronization uses `/dev/rtc1`. |
| RMTFS activation | Validated | The udev rule starts `rmtfs.service` when shared memory appears. |
| Wi-Fi WoWLAN policy | Static candidate | Requests only disconnect and magic-packet wake triggers; suspend retention and wake frequency require Nabu hardware testing. |
| Sensor session gate | Static candidate | Waits for SSC accelerometer enumeration and performs at most one bounded iio-sensor-proxy restart without blocking graphical fallback. |
| Colour temperature | Static candidate | TCS3701 `cct_front` is decoded by libssc and published as standard IIO `in_colortemp_raw`; kernel/COPR/HIL gates remain. |
| SAR hold-awake | Static candidate | Existing Plasma/GNOME controls use calibrated ADUX1050 state and a fail-closed logind inhibitor; physical thresholds remain to be calibrated. |

The default Plasma profile keeps automatic brightness disabled. The measured
curve is retained under `plasma/experimental/` for continued calibration.

## Repository layout

- `alsa/ucm2/`: Nabu ALSA UCM profile. Speaker routing is experimental.
- `plasma/rotation-60hz/`: validated 60 Hz and always-on rotation profiles.
- `plasma/experimental/auto-brightness/`: current calibration experiment.
- `systemd/`: PMIC RTC synchronization service and helper.
- `networkmanager/`: Nabu-specific WCN3990 WoWLAN defaults.
- `desktop/nabu-tablet-controls/`: the existing Plasma widget and GNOME Quick
  Settings extension, backed by shared Nabu helpers rather than compositor
  forks.
- `services/nabu-sar-service/`: the ADUX1050 D-Bus service, guided calibration
  capture tool, fail-closed logind inhibitor and TCS3701-to-IIO bridge.
- `packaging/hexagonrpc-nabu/`: current upstream HexagonRPC plus the pinned,
  checksummed Hotdog/Nabu FastRPC merge layer.
- `packaging/libssc-nabu/`: current upstream libssc plus the Nabu SLPI failure
  recovery patch and compatibility package names.
- `packaging/iio-sensor-proxy-nabu/`: stable upstream SensorProxy plus the
  checksum-locked Nabu SSC discovery, hotplug and accelerometer merge layer.
- `packaging/update-upstreams.sh`: checks new stable upstream tags and prepares
  an update only when every Nabu patch still applies in sequence.
- `packaging/build-srpms.sh`: verifies source locks and produces reproducible
  SRPM inputs for COPR.
- `udev/`: Qualcomm RMTFS activation rule.
- `tests/`: static validation for profiles and scripts.

Physical qualification for the current candidate is documented in
[`docs/NABU-V1.4.0.7-HARDWARE-TESTS.md`](docs/NABU-V1.4.0.7-HARDWARE-TESTS.md).

## Integration

Copy the UCM hierarchy below `alsa/ucm2/` to `/usr/share/alsa/ucm2/`. Install
the RTC service in `/usr/lib/systemd/system/`, its helper as
`/usr/local/libexec/nabu-pmic-rtc-sync`, and the udev rule in
`/usr/lib/udev/rules.d/`.

KWin output UUIDs are generated per environment. Treat the Plasma JSON files
as validated references or merge their Nabu-specific fields into the target
system's existing `kwinoutputconfig.json`; do not replace an unrelated output
configuration blindly.

Run the static checks with:

```sh
./tests/validate.sh
```

Check the pinned upstream sources and merge patches without changing files:

```sh
./packaging/update-upstreams.sh --check
```

Prepare updates to the newest stable tags, then build SRPMs:

```sh
./packaging/update-upstreams.sh --update
./packaging/build-srpms.sh
```

The scheduled GitHub workflow runs the same merge gate and opens a reviewable
update pull request. Merging it triggers SRPM construction; COPR submission is
also automatic when the repository has a `COPR_CONFIG` Actions secret.

## Safety and contribution policy

Hardware reports must identify whether a result was observed during static
validation, boot, or physical testing. Do not commit firmware, private keys,
password databases, credentials, generated kernels, modules, or disk images.

## License

The project is distributed under the MIT License. Existing authorship and
source attribution for derived files is preserved in `NOTICE` and in the file
headers.
