// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    required property string title
    required property string value
    Layout.fillWidth: true
    spacing: 0

    QQC2.Label {
        Layout.fillWidth: true
        text: title
        font.bold: true
    }
    QQC2.Label {
        Layout.fillWidth: true
        text: value
        color: Kirigami.Theme.disabledTextColor
        wrapMode: Text.WordWrap
    }
}
