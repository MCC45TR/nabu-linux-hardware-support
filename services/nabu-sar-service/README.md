# Nabu ADUX1050 SAR service

This service publishes the real three-channel ADUX1050 SSC stream on the
system D-Bus. It never maps SAR to the screen-proximity API.

The optional hold-awake feature uses a standard `systemd-logind` inhibitor.
It is fail closed: the user toggle, a calibrated mapping, a fresh sample, and
the `held` state must all be true. Losing samples for three seconds closes the
inhibitor file descriptor automatically. The toggle defaults off and is stored
under the service-owned `/var/lib/nabu-sar` state directory.

`nabu-sar-control status` is unprivileged and machine readable. Changing the
toggle is performed through the root-only D-Bus method by a polkit-launched
`/usr/libexec/nabu-sar-control set hold-awake on|off` command.

Use `nabu-sar-capture PHASE SECONDS OUTPUT.csv` for controlled HIL calibration.
Do not enable `Mapping.Enabled` until uncovered and held samples have produced
separable thresholds on every intended grip edge.

The companion CCT bridge opens the firmware `cct_front` stream through
libssc, rejects non-finite and out-of-range data, and feeds Kelvin values into
the kernel's `IIO_COLORTEMP` endpoint. Consumers therefore read the standard
`in_colortemp_raw` ABI instead of a desktop-specific interface.
