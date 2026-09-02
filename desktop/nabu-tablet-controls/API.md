# Nabu flashlight user API

Applications may call `nabu-flashlightctl` with `status`, `on [1-100]`,
`off`, `toggle [1-100]`, or `set 1-100`. Percent values are scaled only to
the kernel LED-class `max_brightness` value for the two fixed Nabu torch
channels. The helper cannot select another sysfs path and does not expose the
high-current camera strobe operation.

USB-C role status is available with `nabu-usb-role status`. Role changes use
`pkexec /usr/libexec/nabu-usb-role set data host|device` or
`pkexec /usr/libexec/nabu-usb-role set power source|sink`; the kernel TCPM
driver and the connected partner may reject a role swap.

`pkexec /usr/libexec/nabu-usb-role set mode gadget` switches the data role to
device and starts `nabu-usb-gadget.service`. The composite gadget exports the
primary UID 1000 home directory through MTP, a USB-NCM network with the tablet
at `10.55.0.1`, and a login-protected ACM serial console. It starts OpenSSH for
the normal shell channel over USB and stops it again when sharing is disabled
if the service was not already running. `set mode host` stops and removes
the owned `senemos-nabu` ConfigFS gadget before requesting the host data role;
it does not change the independently controlled USB power role.

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
while a calibrated, fresh sample says the tablet is held.

The feature is disabled when the mapping is uncalibrated, unavailable, stale,
or unknown. SAR is not exposed as display proximity.
