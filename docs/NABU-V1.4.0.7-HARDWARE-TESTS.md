# Nabu v1.4.0.7 hardware qualification

This runbook qualifies the PM8150 reboot modes, ath10k SNOC wake capability,
HexagonRPC file operations, and the bounded SensorProxy startup gate. Passing
source, RPM, or image checks is not evidence that a physical test passed.

## Safety baseline

1. Discover the tablet's current address again; do not reuse an address from a
   previous session.
2. Record `uname -r`, the installed Nabu RPM NEVRAs, `bootctl list`, and SHA-256
   hashes for every EFI entry. In particular, retain a byte-identical Android
   entry and one previously booted Linux fallback.
3. Install only the new kernel, HexagonRPC, and system-integration package set.
   A completed DNF transaction is not a successful boot.
4. Do not delete the old kernel, UKI, module tree, or Android EFI entry during
   this qualification.

## Post-boot identity

- Confirm `uname -r` is `6.17.0-nabu-senemos-v1.4.0.7` and that the running
  `/boot/vmlinuz-*` is owned by `senemos-nabu-kernel-core`.
- Confirm the canonical UKI names that same release and the Android/fallback
  hashes are unchanged.
- Capture `journalctl -b -p warning..alert`, `dmesg`, `systemd-analyze critical-chain
  graphical.target`, and the relevant package NEVRAs before functional tests.

## PM8150 reboot modes

Perform these only after the Android and Linux fallback paths are verified.

1. Request the bootloader reboot mode and verify that the tablet enters
   fastboot rather than rebooting normally or hanging. Return to Linux without
   flashing anything.
2. Request the recovery reboot mode and verify that the installed recovery path
   is selected. Return through the known-safe boot path.
3. Perform three ordinary reboots and one power-off/power-on cycle to ensure the
   mode does not remain latched.
4. Save PON/reboot logs if any request falls back to a normal reboot.

## Wi-Fi suspend and WoWLAN

1. Verify the WCN3990 device exposes wakeup as enabled under sysfs and inspect
   `iw phy ... wowlan show`. NetworkManager should request only `disconnect`
   and `magic-packet`, never `any`.
2. Complete 15 suspend/resume cycles while associated. Record association,
   DHCP address, gateway reachability, signal strength, suspend duration, and
   wake reason after each cycle.
3. Repeat five cycles with idle networking and five with controlled background
   traffic. Unexpected immediate wakes are a failure.
4. Send one magic packet from another host and verify that it wakes the tablet;
   ordinary broadcast traffic must not do so.
5. Check ath10k, QRTR, RMTFS, QMI and firmware logs for crashes, repeated
   reconnect loops, key timeouts, or lost firmware services.

## HexagonRPC and sensors

1. Verify `hexagonrpcd-sdsp.service`, `iio-sensor-proxy.service`, and
   `nabu-sensor-session-gate.service`. The gate may restart SensorProxy at most
   once and must always finish successfully within 30 seconds.
2. Confirm `HasAccelerometer=true`, automatic rotation in all four
   orientations, and the existing ambient-light behavior before and after ten
   suspend/resume cycles.
3. Confirm the desktop still opens when SensorProxy is deliberately unavailable
   in a controlled test. This is the required fail-open rollback path; restore
   the service immediately afterward.
4. Capture SDSP/SLPI remoteproc and FastRPC logs. There must be no crash loop,
   use-after-free, path escape, or permission regression.
5. Hash the Nabu `hexagonfs` tree before and after testing. Expected DSP registry
   updates may use write/remove/rename, but firmware or calibration files
   outside that restricted root must not change.

## Acceptance and rollback

Accept the candidate only if the running release is correct, Android/fallback
hashes remain identical, both reboot modes work, 15 Wi-Fi cycles complete
without a suspend regression, sensors remain available, and the fail-open
desktop path works. Otherwise boot the retained Linux fallback and downgrade
the three package groups to their previous NEVRAs; preserve the failed-boot
journal and hashes for diagnosis.

## Deferred work

- SMB5/PM8150B charging is intentionally not changed in v1.4.0.7. Preserve the
  Hotdog method as a future rebase reference: patches `0150` through `0155` in
  [`aports/main/linux-postmarketos-qcom-sm8150`](https://github.com/Sr-0w/hotdog-linux-bringup/tree/8ab7a510eec3614fb3021bf2595c0f9e475009bb/aports/main/linux-postmarketos-qcom-sm8150).
- Camera is the final bring-up target. Reuse the Hotdog CAMSS/CCI integration
  method only after Nabu's own camera topology, regulators, clocks, GPIOs and
  actual sensors are identified. Do not import Hotdog's sensor DTS nodes as if
  the modules were shared.
