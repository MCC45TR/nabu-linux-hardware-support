import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';
import * as QuickSettings from 'resource:///org/gnome/shell/ui/quickSettings.js';
import {Extension, gettext as _} from 'resource:///org/gnome/shell/extensions/extension.js';

const FLASHLIGHT = '/usr/libexec/nabu-flashlight';
const USB_ROLE = '/usr/libexec/nabu-usb-role';
const USB_GADGET = '/usr/libexec/nabu-usb-gadget';
const ACCESSORIES = '/usr/libexec/nabu-accessory-state';
const SAR_CONTROL = '/usr/libexec/nabu-sar-control';
const SLIDER_ANIMATION_MS = 180;

function run(argv, callback) {
    try {
        const proc = Gio.Subprocess.new(argv,
            Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_SILENCE);
        proc.communicate_utf8_async(null, null, (process, result) => {
            try {
                const [, stdout] = process.communicate_utf8_finish(result);
                callback(process.get_successful(), stdout.trim());
            } catch (_) {
                callback(false, '');
            }
        });
    } catch (_) {
        callback(false, '');
    }
}

function value(output, key, fallback = '') {
    return output.match(new RegExp(`(?:^|\\n)${key}=([^\\n]+)`))?.[1]?.trim() ?? fallback;
}

const FlashlightSlider = GObject.registerClass(
class FlashlightSlider extends QuickSettings.QuickSlider {
    _init(indicator) {
        super._init({
            iconName: 'flashlight-on-symbolic',
            iconLabel: _('Flashlight brightness'),
        });
        this._indicator = indicator;
        this.slider.accessible_name = _('Flashlight brightness');
        this.slider.value = 0.13;
        this._changedId = this.slider.connect('notify::value', () => {
            this._indicator.queueFlashlightLevel(Math.max(1,
                Math.round(this.slider.value * 100)));
        });
    }

    setPercent(percent) {
        this.slider.block_signal_handler(this._changedId);
        this.slider.value = Math.max(1, percent) / 100;
        this.slider.unblock_signal_handler(this._changedId);
    }
});

const SoundSlider = GObject.registerClass(
class SoundSlider extends QuickSettings.QuickSlider {
    _init(source) {
        super._init({
            iconName: 'audio-volume-high-symbolic',
            iconLabel: _('Sound volume'),
        });
        this.slider.accessible_name = _('Sound volume');
        this._source = source;
        this._syncing = false;
        this._sliderId = this.slider.connect('notify::value', () => this._apply());
        this._sourceId = this._source.slider.connect('notify::value', () => this._syncValue());
        this._iconId = this._source.connect('notify::icon-name', () => this._syncValue());
        this._syncValue();
    }

    _syncValue() {
        this._syncing = true;
        this.slider.value = this._source.slider.value;
        this._syncing = false;
        this.iconName = this._source.iconName;
    }

    _apply() {
        if (this._syncing)
            return;
        this._source.slider.value = this.slider.value;
    }

    destroy() {
        if (this._sliderId)
            this.slider.disconnect(this._sliderId);
        if (this._sourceId)
            this._source.slider.disconnect(this._sourceId);
        if (this._iconId)
            this._source.disconnect(this._iconId);
        super.destroy();
    }
});

const TabletIndicator = GObject.registerClass(
class TabletIndicator extends QuickSettings.SystemIndicator {
    _init(settings) {
        super._init();
        this._settings = settings;
        this._flashlightOn = false;
        this._flashlightLevel = 13;
        this._flashlightRequestedLevel = 13;
        this._flashlightApplyInFlight = false;
        this._flashlightFrameId = 0;
        this._laters = global.compositor.get_laters();
        this._powerSettings = null;
        this._settingsSignal = this._settings.connect('changed', () => this.refresh());
        this._powerSettingsSignal = 0;
        this._buildItems();
        this.refresh();
    }

    _enabled(key) {
        return this._settings.get_boolean(key);
    }

    _toggle(title, iconName, callback) {
        const item = new QuickSettings.QuickToggle({title, iconName, toggleMode: true});
        item.visible = false;
        item.connect('clicked', callback);
        this.quickSettingsItems.push(item);
        return item;
    }

    _buildItems() {
        this._flashlight = this._toggle(_('Flashlight'), 'flashlight-off-symbolic', () => {
            const command = this._flashlightOn
                ? [FLASHLIGHT, 'off']
                : [FLASHLIGHT, 'on', String(this._flashlightLevel)];
            this._flashlightOn = !this._flashlightOn;
            this._flashlight.checked = this._flashlightOn;
            this._setFlashlightSliderVisible(this._flashlightOn);
            run(command, () => this._refreshFlashlight());
        });

        this._autoBrightness = this._toggle(_('Automatic brightness'),
            'display-brightness-symbolic', () => {
                if (this._powerSettings)
                    this._powerSettings.set_boolean('ambient-enabled', this._autoBrightness.checked);
            });
        this._setupAutoBrightness();

        this._usbSharing = this._toggle(_('USB device sharing'),
            'network-wired-symbolic', () => {
                const mode = this._usbSharing.checked ? 'gadget' : 'off';
                run(['pkexec', USB_ROLE, 'set', 'mode', mode], () => this._refreshUsb());
            });
        this._usbData = this._toggle(_('USB data role'),
            'drive-removable-media-usb-symbolic', () => {
                const mode = this._usbData.checked ? 'host' : 'gadget';
                run(['pkexec', USB_ROLE, 'set', 'mode', mode], () => this._refreshUsb());
            });

        this._usbPower = new QuickSettings.QuickMenuToggle({
            title: _('USB power role'),
            iconName: 'battery-symbolic',
            toggleMode: false,
        });
        this._usbPower.visible = false;
        this._usbPower.menu.setHeader('battery-symbolic', _('USB power role'),
            _('Choose how the USB-C power role is selected'));
        for (const [label, role] of [
            [_('Automatic'), 'auto'],
            [_('Power source'), 'source'],
            [_('Power sink'), 'sink'],
        ]) {
            const choice = new PopupMenu.PopupMenuItem(label);
            choice.connect('activate', () => {
                run(['pkexec', USB_ROLE, 'set', 'power', role], () => this._refreshUsb());
            });
            this._usbPower.menu.addMenuItem(choice);
        }
        this.quickSettingsItems.push(this._usbPower);

        this._pen = this._toggle(_('Xiaomi Smart Pen'), 'input-tablet-symbolic', () => {});
        this._pen.toggleMode = false;
        this._pen.reactive = false;
        this._keyboard = this._toggle(_('Xiaomi Keyboard'), 'input-keyboard-symbolic', () => {});
        this._keyboard.toggleMode = false;
        this._keyboard.reactive = false;

        this._sarHoldAwake = this._toggle(_('Keep awake while held'),
            'preferences-system-time-symbolic', () => {
                if (!this._sarHoldAwake.reactive)
                    return;
                const enabled = this._sarHoldAwake.checked ? 'on' : 'off';
                run(['pkexec', SAR_CONTROL, 'set', 'hold-awake', enabled],
                    () => this._refreshSar());
            });

        this._flashlightSlider = new FlashlightSlider(this);
        this._flashlightSlider.visible = false;
        this._soundSlider = null;
    }

    attachSliders(menu, quickSettings) {
        const brightness = quickSettings._brightness?.quickSettingsItems?.[0] ?? null;
        const nativeVolume = quickSettings._volumeOutput?.quickSettingsItems?.[0] ?? null;
        if (!nativeVolume)
            return;
        this._soundSlider = new SoundSlider(nativeVolume);
        if (brightness) {
            const sibling = brightness.get_next_sibling();
            menu.insertItemBefore(this._soundSlider, sibling, 2);
        } else {
            menu.addItem(this._soundSlider, 2);
        }
        this._nativeVolume = nativeVolume;
        this._nativeVolumeWasVisible = nativeVolume.visible;
        nativeVolume.hide();
        menu.addItem(this._flashlightSlider, 2);
    }

    _setupAutoBrightness() {
        const shellQuickSettings = Main.panel.statusArea.quickSettings;
        if (shellQuickSettings._autoBrightness || shellQuickSettings._ambientBrightness)
            return;
        const source = Gio.SettingsSchemaSource.get_default();
        const schema = source?.lookup('org.gnome.settings-daemon.plugins.power', true);
        if (!schema?.has_key('ambient-enabled'))
            return;
        this._powerSettings = new Gio.Settings({settings_schema: schema});
        const sync = () => {
            this._autoBrightness.checked = this._powerSettings.get_boolean('ambient-enabled');
            this._autoBrightness.subtitle = null;
        };
        this._powerSettingsSignal = this._powerSettings.connect('changed::ambient-enabled', sync);
        sync();
    }

    _setFlashlightSliderVisible(visible, animate = true) {
        visible = visible && this._enabled('show-flashlight');
        if (visible) {
            this._flashlightSlider.show();
            this._flashlightSlider.ease({
                opacity: 255,
                duration: animate ? SLIDER_ANIMATION_MS : 0,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            });
        } else if (this._flashlightSlider.visible) {
            this._flashlightSlider.ease({
                opacity: 0,
                duration: animate ? SLIDER_ANIMATION_MS : 0,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => this._flashlightSlider.hide(),
            });
        }
    }

    queueFlashlightLevel(percent) {
        this._flashlightLevel = percent;
        this._flashlightRequestedLevel = percent;
        if (!this._flashlightOn || this._flashlightFrameId || this._flashlightApplyInFlight)
            return;
        this._flashlightFrameId = this._laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
            this._flashlightFrameId = 0;
            this._applyFlashlightLevel();
            return GLib.SOURCE_REMOVE;
        });
    }

    _applyFlashlightLevel() {
        if (!this._flashlightOn || this._flashlightApplyInFlight)
            return;
        const applied = this._flashlightRequestedLevel;
        this._flashlightApplyInFlight = true;
        run([FLASHLIGHT, 'set', String(applied)], () => {
            this._flashlightApplyInFlight = false;
            if (this._flashlightOn && applied !== this._flashlightRequestedLevel)
                this.queueFlashlightLevel(this._flashlightRequestedLevel);
        });
    }

    _refreshFlashlight() {
        run([FLASHLIGHT, 'status'], (success, output) => {
            this._flashlight.visible = success && this._enabled('show-flashlight');
            if (!success) {
                this._setFlashlightSliderVisible(false);
                return;
            }
            const fields = output.split(/\s+/);
            this._flashlightOn = fields[0] === 'on';
            const level = Number(fields[1]);
            if (this._flashlightOn && Number.isFinite(level) && level > 0)
                this._flashlightLevel = level;
            this._flashlightRequestedLevel = this._flashlightLevel;
            this._flashlight.checked = this._flashlightOn;
            this._flashlight.subtitle = this._flashlightOn
                ? _('On at %1%').replace('%1', this._flashlightLevel) : _('Off');
            this._flashlight.iconName = this._flashlightOn
                ? 'flashlight-on-symbolic' : 'flashlight-off-symbolic';
            this._flashlightSlider.setPercent(this._flashlightLevel);
            this._setFlashlightSliderVisible(this._flashlightOn);
        });
    }

    _refreshUsb() {
        run([USB_ROLE, 'status'], (success, output) => {
            const available = success && output.includes('type=');
            this._usbData.visible = available && this._enabled('show-usb-data-role');
            this._usbPower.visible = available && this._enabled('show-usb-power-role');
            if (!available)
                return;
            const dataRole = output.match(/data=[^\n]*\[([^\]]+)\]/)?.[1] ?? 'unknown';
            const powerRole = output.match(/power=[^\n]*\[([^\]]+)\]/)?.[1] ?? 'unknown';
            const portType = output.match(/type=[^\n]*\[([^\]]+)\]/)?.[1] ?? 'dual';
            this._usbData.checked = dataRole === 'host';
            this._usbData.subtitle = dataRole === 'host' ? _('Host mode') : _('Device mode');
            this._usbPower.subtitle = portType === 'dual' ? _('Automatic')
                : portType === 'source' ? _('Power source') : _('Power sink');
            this._usbPower.checked = portType !== 'dual' || powerRole === 'source';
        });
        run([USB_GADGET, 'status'], (success, output) => {
            this._usbSharing.visible = success && this._enabled('show-usb-sharing');
            this._usbSharing.checked = value(output, 'gadget', 'inactive') === 'active';
            this._usbSharing.subtitle = this._usbSharing.checked ? _('Active') : _('Inactive');
        });
    }

    _refreshAccessories() {
        run([ACCESSORIES, 'status'], (success, output) => {
            const connected = success && value(output, 'pen_connected', '0') === '1';
            const battery = Number(value(output, 'pen_battery', '-1'));
            const charging = value(output, 'pen_charging', '0') === '1';
            this._pen.visible = connected && this._enabled('show-pen');
            this._pen.checked = connected;
            this._pen.title = connected
                ? value(output, 'pen_name', _('Xiaomi Smart Pen')) : _('Xiaomi Smart Pen');
            const penState = [_('Connected')];
            if (battery >= 0)
                penState.push(_('Battery: %1%').replace('%1', battery));
            if (charging)
                penState.push(_('Charging'));
            this._pen.subtitle = penState.join(' · ');

            const attached = success && value(output, 'keyboard_attached', '0') === '1';
            this._keyboard.visible = attached && this._enabled('show-keyboard');
            this._keyboard.subtitle = _('Attached');
        });
    }

    _refreshSar() {
        run([SAR_CONTROL, 'status'], (success, output) => {
            const sensorAvailable = success && value(output, 'available', '0') === '1';
            const calibrated = success && value(output, 'mapping_enabled', '0') === '1';
            const enabled = success && value(output, 'hold_awake_enabled', '0') === '1';
            const inhibited = success && value(output, 'sleep_inhibited', '0') === '1';
            const gripState = value(output, 'grip_state', 'unknown');
            this._sarHoldAwake.visible = success && this._enabled('show-hold-awake');
            this._sarHoldAwake.reactive = sensorAvailable && calibrated;
            this._sarHoldAwake.checked = enabled;
            if (!calibrated)
                this._sarHoldAwake.subtitle = _('Calibration required');
            else if (!enabled)
                this._sarHoldAwake.subtitle = _('Inactive');
            else if (inhibited)
                this._sarHoldAwake.subtitle = _('Held · sleep blocked');
            else if (gripState === 'held')
                this._sarHoldAwake.subtitle = _('Held · acquiring inhibitor');
            else if (gripState === 'released')
                this._sarHoldAwake.subtitle = _('Waiting until held');
            else
                this._sarHoldAwake.subtitle = _('Waiting for sensor data');
        });
    }

    refresh() {
        this._autoBrightness.visible = this._powerSettings !== null &&
            this._enabled('show-auto-brightness');
        if (this._soundSlider)
            this._soundSlider.visible = this._enabled('show-sound-slider');
        this._refreshFlashlight();
        this._refreshUsb();
        this._refreshAccessories();
        this._refreshSar();
    }

    destroy() {
        if (this._flashlightFrameId)
            this._laters.remove(this._flashlightFrameId);
        if (this._powerSettings && this._powerSettingsSignal)
            this._powerSettings.disconnect(this._powerSettingsSignal);
        if (this._settingsSignal)
            this._settings.disconnect(this._settingsSignal);
        if (this._nativeVolume && this._nativeVolumeWasVisible)
            this._nativeVolume.show();
        this._flashlightSlider.destroy();
        this._soundSlider?.destroy();
        this.quickSettingsItems.forEach(item => item.destroy());
        super.destroy();
    }
});

export default class NabuTabletControlsExtension extends Extension {
    enable() {
        this._settings = this.getSettings();
        this._indicator = new TabletIndicator(this._settings);
        const quickSettings = Main.panel.statusArea.quickSettings;
        quickSettings.addExternalIndicator(this._indicator, 1);
        this._indicator.attachSliders(quickSettings.menu, quickSettings);
        this._menuSignal = quickSettings.menu.connect(
            'open-state-changed', (_menu, open) => {
                if (open)
                    this._indicator?.refresh();
            });
    }

    disable() {
        if (this._menuSignal)
            Main.panel.statusArea.quickSettings.menu.disconnect(this._menuSignal);
        this._menuSignal = 0;
        this._indicator?.destroy();
        this._indicator = null;
        this._settings = null;
    }
}
