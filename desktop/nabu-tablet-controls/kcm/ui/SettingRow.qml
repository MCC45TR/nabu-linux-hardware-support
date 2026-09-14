// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: row
    required property string title
    required property string description
    required property string iconName
    property bool known: true
    property bool available: true
    property bool checked: false
    signal toggled(bool checked)

    Layout.fillWidth: true
    spacing: Kirigami.Units.largeSpacing

    Kirigami.Icon {
        source: row.iconName
        implicitWidth: Kirigami.Units.iconSizes.medium
        implicitHeight: implicitWidth
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 0

        Controls.Label {
            Layout.fillWidth: true
            text: row.title
            font.bold: true
            wrapMode: Text.WordWrap
        }
        Controls.Label {
            Layout.fillWidth: true
            text: !row.known ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Checking hardware…") : row.description
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.WordWrap
        }
    }

    Controls.Switch {
        checked: row.checked
        enabled: row.known && row.available
        onToggled: row.toggled(checked)
        Accessible.name: row.title
    }
}
