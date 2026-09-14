# Nabu tablet user API

Applications may call `nabu-flashlightctl` with `status`, `on [1-100]`,
`off`, `toggle [1-100]`, or `set 1-100`. Percent values are scaled only to
the kernel LED-class `max_brightness` value for the two fixed Nabu torch
channels. The helper cannot select another sysfs path and does not expose the
high-current camera strobe operation.

If a camera service has the kernel flash subdevice open, the LED class rejects
sysfs writes with `EBUSY`. In that case the helper discovers only the fixed
`white:flash` and `yellow:flash` V4L2 subdevices and applies the low-current
torch controls through V4L2. It refuses to replace a flash mode already armed
by a camera and always rolls all matched torch channels back off after a
partial failure.

USB-C role status is available with `nabu-usb-role status`. Role changes use
`pkexec /usr/libexec/nabu-usb-role set data host|device` or
`pkexec /usr/libexec/nabu-usb-role set power auto|source|sink`. These choices
write the Type-C `port_type` policy, so source mode can be prepared before an
ESP32 or other peripheral is attached. The kernel TCPM driver may still reject
a policy that the port hardware cannot support.

`pkexec /usr/libexec/nabu-usb-role set mode gadget` switches the data role to
device and starts `nabu-usb-gadget.service`. The composite gadget exports the
primary UID 1000 home directory through MTP, a USB-NCM network with the tablet
at `10.55.0.1`, and a login-protected ACM serial console. It starts OpenSSH for
the normal shell channel over USB and stops it again when sharing is disabled
if the service was not already running. `set mode host` stops and removes
the owned `senemos-nabu` ConfigFS gadget before requesting the host data role;
it does not change the independently controlled USB power role. On host-only
installations where `nabu-usb-gadget.service` is not shipped, host and off mode
treat the already-absent gadget as stopped. An installed gadget service that
fails to stop remains a hard error and blocks the role change.

The profile reserves a FunctionFS ADB function, but enables it only when a
Linux `/usr/sbin/adbd` is installed and `/adb_keys` contains at least one
administrator-approved host public key. It never enables an unauthenticated
ADB root shell. `nabu-usb-gadget status` reports the active UDC and the MTP,
ADB, NCM, serial and SSH state.

`nabu-accessory-state status` reads the Nabu IDTP9418 pen power-supply
attributes, the pogo-keyboard driver's `connected` attribute, and paired pen
state from BlueZ. It reports line-oriented `key=value` data without running a
daemon or polling service. A pen is exposed to the widget only after it has
been paired at least once; `nabu-accessory-state connect MAC` asks BlueZ to
connect that already-paired pen. The keyboard indicator is exposed only while
the kernel reports the pogo keyboard as attached.

The GNOME Shell extension uses the same helpers and exposes each available
capability as an independent Quick Settings item. GNOME's built-in automatic
rotation item remains authoritative and is not duplicated. Automatic
brightness is backed by the stock GNOME Settings Daemon `ambient-enabled`
setting when that key exists. State is refreshed when Quick Settings opens and
after actions; no persistent polling service is installed. A one-shot user
unit enables the extension on the first GNOME login and records completion in
the user's state directory, so a later manual disable is preserved.
# Grip-aware hold-awake

`/usr/libexec/nabu-sar-control status` reports the real ADUX1050 state as
machine-readable key/value pairs. The existing Plasma widget and GNOME Quick
Settings extension invoke `pkexec /usr/libexec/nabu-sar-control set hold-awake
on|off`; polkit authorizes only that fixed helper. The root SAR service owns
the persistent toggle and obtains a standard systemd-logind inhibitor only
while a calibrated, fresh and validated-changing sample says the tablet is
held. `sample_fresh` proves only that SSC transport is alive; `sample_quality`,
`data_usable`, `data_changing`, `consecutive_identical_samples`, and
`saturated_channel_mask` prevent constant or saturated reports from being
treated as physical grip evidence.

The feature is disabled when the mapping is uncalibrated, unavailable, stale,
or unknown. SAR is not exposed as display proximity.

# Motion, gesture, and grip diagnostics

The Plasma widget reads the same unprivileged status command to show three
different SSC gates: a data type advertised by firmware, an event-driven stream
whose enable request was accepted, and a known data report actually observed
for that SUID. An advertised endpoint is never labelled as event delivery.
Continuous or unknown-rate endpoints are listed but not enabled by the
background daemon.

The grip row exposes fresh CH0/CH1/CH2 delta, raw, and baseline values together
with the configured mask, thresholds, and debounce count. These values are
diagnostics only. The classifier and logind inhibitor remain disabled until
controlled uncovered/held HIL establishes device-specific thresholds.

The System Settings page can subscribe directly to the SAR and SSC
`PropertiesChanged` signals. Live view is opt-in and has no polling timer or
periodic subprocess; stopping it disconnects both signal subscriptions. The
two-phase grip calibration collects ten released and ten held samples only in
memory, rejects overlapping ranges, and presents conservative hysteresis
thresholds for explicit administrator approval. The privileged C++ helper
writes only `/etc/nabu-sar.conf` atomically, validates every numeric field,
restarts only `nabu-sar-service.service`, and restores the previous file if the
restart request fails. It never writes firmware, EEPROM, modem NV or an Android
partition.

# Wake gestures and Plasma settings

`plasma-nabu-kcm` installs the Nabu Tablet page in Plasma System Settings and
owns the matching System Tray widget. The widget keeps only everyday controls
in its popup; advertised, monitored, and physically observed Sensor DSP states
are separated on the widget's Information configuration page.

The KCM mirrors every widget control: flashlight and brightness, display
rotation and ambient brightness, magnetic-cover sleep, double-tap and tilt
wake, calibrated grip hold-awake, Smart Pen connection and battery state, pogo
keyboard presence, USB sharing, data role, and power role. It links to KDE's
native Display, Color Profiles, Night Light, Power Management, Drawing Tablet,
Bluetooth, and Keyboard pages for standard settings instead of duplicating
those implementations. Both interfaces share one gettext domain and use
Kirigami/Qt Quick Controls so Breeze colors, metrics, keyboard navigation, and
accessibility remain authoritative.

`/usr/libexec/nabu-wake-control status` is unprivileged and reports capability
and state. Its allowlisted `set double-tap on|off` operation writes only the
documented NT36523 `double_tap_to_wake` attribute. The kernel exposes that
attribute only when Device Tree declares both the gesture and touchscreen wake
source. The current policy is retained under `/var/lib/nabu-wake` and restored
after boot without changing the firmware-provided default on first install.

Tilt wake is opt-in. `nabu-wake-service` watches the existing read-only
`org.senemos.Nabu.Sensors1` algorithm report counters and emits only a standard
`KEY_WAKEUP` event through a dedicated uinput device after a new
`tilt_to_wake` report. Its systemd device policy grants access only to
`/dev/uinput`; the service has no block-device or firmware-partition access.
The option remains unavailable if either the SSC endpoint or the kernel uinput
device is missing.
