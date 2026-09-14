// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasma5support as Plasma5Support

QQC2.ScrollView {
    id: page
    contentWidth: availableWidth
    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    property string summary: i18n("Reading Sensor DSP state…")
    property string availableAlgorithms: ""
    property string monitoredAlgorithms: ""
    property string reportedAlgorithms: ""
    property string freshAlgorithms: ""
    property string discoveredOnlyAlgorithms: ""
    property string gripTelemetry: i18n("Reading grip channels…")
    property string cameraCalibration: i18n("Runtime report is checked by Nabu Tablet Settings")
    property string errorText: ""
    property int serial: 0

    function value(output, key, fallback) {
        const match = output.match(new RegExp("(?:^|\\n)" + key + "=([^\\n]+)"))
        return match ? match[1].trim() : fallback
    }

    function label(dataType) {
        switch (dataType) {
        case "tilt_to_wake": return i18n("Tilt to wake")
        case "tilt": return i18n("Tilt")
        case "pickup": return i18n("Pickup")
        case "screen_down": return i18n("Screen down")
        case "pedometer": return i18n("Pedometer")
        case "basic_gestures": return i18n("Basic gestures")
        case "bring_to_ear": return i18n("Bring to ear")
        case "multishake": return i18n("Multi-shake")
        case "device_orient": return i18n("Device orientation")
        case "facing": return i18n("Facing")
        case "sig_motion": return i18n("Significant motion")
        case "motion_detect": return i18n("Motion detect")
        case "oem_step_detector": return i18n("OEM step detector")
        case "gravity": return i18n("Gravity")
        case "sar_algo_1": return i18n("SAR algorithm")
        case "psmd": return i18n("Persistent significant motion")
        case "3d_signature": return i18n("3D signature")
        default: return dataType
        }
    }

    function list(csv) {
        return csv.length ? csv.split(",").map(item => label(item)).join(", ") : i18n("None")
    }

    function refresh() {
        const source = "env NABU_WIDGET_INFO_REQUEST=" + (++serial) + " /usr/libexec/nabu-sar-control status"
        executable.connectSource(source)
    }

    function apply(output) {
        const total = value(output, "algorithm_total", "0")
        const available = value(output, "algorithm_available_count", "0")
        const monitored = value(output, "algorithm_monitoring_count", "0")
        const reporting = value(output, "algorithm_reporting_count", "0")
        summary = i18n("%1/%2 firmware endpoints available · %3 monitored · %4 reporting",
            available, total, monitored, reporting)
        availableAlgorithms = list(value(output, "algorithm_available", ""))
        monitoredAlgorithms = list(value(output, "algorithm_monitoring", ""))
        reportedAlgorithms = list(value(output, "algorithm_reports", ""))
        freshAlgorithms = list(value(output, "algorithm_fresh", ""))
        discoveredOnlyAlgorithms = list(value(output, "algorithm_discovered_only", ""))
        gripTelemetry = value(output, "sample_fresh", "0") === "1"
            ? i18n("Fresh ADUX1050 deltas: %1 · raw: %2 · baseline: %3",
                value(output, "deltas", ""), value(output, "raw_values", ""), value(output, "baselines", ""))
            : i18n("No fresh ADUX1050 sample")
        if (value(output, "mapping_enabled", "0") !== "1")
            gripTelemetry += " · " + i18n("classifier disabled until controlled HIL calibration")
    }

    Plasma5Support.DataSource {
        id: executable
        engine: "executable"
        onNewData: function(source, data) {
            const output = String(data.stdout || "").trim()
            if (data["exit code"] === 0)
                page.apply(output)
            else
                page.errorText = i18n("Sensor diagnostics are currently unavailable.")
            disconnectSource(source)
        }
    }

    Plasma5Support.DataSource {
        id: launcher
        engine: "executable"
        onNewData: function(source, data) { disconnectSource(source) }
    }

    Component.onCompleted: refresh()

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.errorText.length > 0
            type: Kirigami.MessageType.Error
            text: page.errorText
        }

        Kirigami.Heading {
            text: i18n("Sensor DSP")
            level: 2
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: page.summary
            wrapMode: Text.WordWrap
        }

        InfoBlock { title: i18n("Firmware endpoints"); value: page.availableAlgorithms }
        InfoBlock { title: i18n("Safely monitored"); value: page.monitoredAlgorithms }
        InfoBlock { title: i18n("Data reports observed"); value: page.reportedAlgorithms }
        InfoBlock { title: i18n("Fresh reports"); value: page.freshAlgorithms }
        InfoBlock { title: i18n("Discovered without a report yet"); value: page.discoveredOnlyAlgorithms }

        Kirigami.Separator { Layout.fillWidth: true }

        Kirigami.Heading {
            text: i18n("Grip sensor")
            level: 2
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: page.gripTelemetry
            wrapMode: Text.WordWrap
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            text: i18n("Discovery proves that firmware advertised an algorithm. Enable acknowledgement is not a physical test: only a known sensor data message is counted as an observed report. Raw CH0/CH1/CH2 values stay visible while unsafe grip thresholds remain fail-closed.")
            visible: true
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight

            QQC2.Button {
                text: i18n("Open Live Sensors and Calibration")
                icon.name: "input-tablet"
                onClicked: launcher.connectSource("systemsettings kcm_nabu")
            }

            QQC2.Button {
                text: i18n("Refresh information")
                icon.name: "view-refresh"
                onClicked: page.refresh()
            }
        }
    }
}
