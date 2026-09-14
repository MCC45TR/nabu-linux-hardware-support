// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    required property string title
    required property string value
    Layout.fillWidth: true
    spacing: 0

    Controls.Label {
        Layout.fillWidth: true
        text: title
        font.bold: true
    }
    Controls.Label {
        Layout.fillWidth: true
        text: value.length ? value : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Unavailable")
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
    }
}
