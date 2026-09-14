# Nabu SAR and SSC algorithm service

This service publishes the real three-channel ADUX1050 SSC stream on the
system D-Bus. It never maps SAR to the screen-proximity API.

The optional hold-awake feature uses a standard `systemd-logind` inhibitor.
It is fail closed: the user toggle, a calibrated mapping, a fresh transport
sample, validated changing data, and the `held` state must all be true. Losing
samples for three seconds, a constant stream, or a saturated channel closes the
inhibitor file descriptor automatically. The toggle defaults off and is stored
under the service-owned `/var/lib/nabu-sar` state directory.

`SampleFresh` describes transport liveness only. `SampleQuality`, `DataUsable`,
`DataChanging`, `ConsecutiveIdenticalSamples`, and `SaturatedChannelMask`
separate a live SSC connection from physically useful ADUX1050 data. Sixteen
identical saturated reports or thirty-two identical ordinary reports are
rejected without restarting SLPI or writing sensor registers.
After a stream is classified as stuck, unchanged D-Bus telemetry is bounded to
one update every five seconds; quality transitions are still emitted
immediately. This preserves diagnostics without a needless desktop wake-up
every second.

`nabu-sar-control status` is unprivileged and machine readable. Changing the
toggle is performed through the root-only D-Bus method by a polkit-launched
`/usr/libexec/nabu-sar-control set hold-awake on|off` command.

Use `nabu-sar-capture PHASE SECONDS OUTPUT.csv` for controlled HIL calibration.
The capture helper records only samples whose data quality is usable.
Do not enable `Mapping.Enabled` until uncovered and held samples have produced
separable thresholds on every intended grip edge.

The same daemon publishes a read-only `org.senemos.Nabu.Sensors1` inventory at
`/org/senemos/Nabu/Sensors`. It discovers the device's motion and gesture data
types one at a time, and enables only firmware endpoints explicitly reported as
on-change or single-output. Continuous or unknown-rate streams remain
discovery-only so a background service cannot silently create a permanent
high-rate workload. Firmware discovery, enable acknowledgement, and an observed
known sensor-data message are separate properties; none is reported as proof of
another.

Generic monitoring requires libssc with correctly typed `stream-type` and
`available` GObject properties. With an older ABI the daemon detects the type
mismatch before reading either property and safely leaves that endpoint at
`incompatible-libssc-abi` instead of risking a pointer-sized write into scalar
memory.

The companion CCT bridge opens the firmware `cct_front` endpoint through
libssc, rejects non-finite and out-of-range data, and feeds Kelvin values into
the kernel's `IIO_COLORTEMP` endpoint. Consumers therefore read the standard
`in_colortemp_raw` ABI instead of a desktop-specific interface. The firmware
stream is on-change, so the bridge retains the last valid sample until it stops;
startup and shutdown invalidate the channel explicitly.
