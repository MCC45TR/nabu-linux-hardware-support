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

The default Plasma profile keeps automatic brightness disabled. The measured
curve is retained under `plasma/experimental/` for continued calibration.

## Repository layout

- `alsa/ucm2/`: Nabu ALSA UCM profile. Speaker routing is experimental.
- `plasma/rotation-60hz/`: validated 60 Hz and always-on rotation profiles.
- `plasma/experimental/auto-brightness/`: current calibration experiment.
- `systemd/`: PMIC RTC synchronization service and helper.
- `udev/`: Qualcomm RMTFS activation rule.
- `tests/`: static validation for profiles and scripts.

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

## Safety and contribution policy

Hardware reports must identify whether a result was observed during static
validation, boot, or physical testing. Do not commit firmware, private keys,
password databases, credentials, generated kernels, modules, or disk images.

## License

The project is distributed under the MIT License. Existing authorship and
source attribution for derived files is preserved in `NOTICE` and in the file
headers.

