import Adw from 'gi://Adw';
import Gio from 'gi://Gio';
import {ExtensionPreferences, gettext as _} from 'resource:///org/gnome/Shell/Extensions/js/extensions/prefs.js';

const OPTIONS = [
    ['show-flashlight', 'Flashlight', 'Show the flashlight tile and its live brightness slider'],
    ['show-sound-slider', 'Sound slider', 'Show a volume slider below display brightness'],
    ['show-auto-brightness', 'Automatic brightness', 'Show the ambient-light toggle'],
    ['show-usb-sharing', 'USB device sharing', 'Show USB gadget sharing controls'],
    ['show-usb-data-role', 'USB data role', 'Show host and device mode controls'],
    ['show-usb-power-role', 'USB power role', 'Show automatic, source and sink power choices'],
    ['show-pen', 'Xiaomi Smart Pen', 'Show the tile only while the pen is connected'],
    ['show-keyboard', 'Xiaomi Keyboard', 'Show the tile only while the keyboard is attached'],
    ['show-hold-awake', 'Keep awake while held', 'Use the calibrated grip sensor to prevent idle sleep while held'],
];

export default class NabuTabletControlsPreferences extends ExtensionPreferences {
    fillPreferencesWindow(window) {
        const settings = this.getSettings();
        const page = new Adw.PreferencesPage({
            title: _('Tablet Controls'),
            iconName: 'preferences-system-symbolic',
        });
        const group = new Adw.PreferencesGroup({
            title: _('Quick Settings'),
            description: _('Choose which Nabu controls are available in Quick Settings.'),
        });
        page.add(group);
        window.add(page);

        for (const [key, title, subtitle] of OPTIONS) {
            const row = new Adw.SwitchRow({title: _(title), subtitle: _(subtitle)});
            settings.bind(key, row, 'active', Gio.SettingsBindFlags.DEFAULT);
            group.add(row);
        }
    }
}
