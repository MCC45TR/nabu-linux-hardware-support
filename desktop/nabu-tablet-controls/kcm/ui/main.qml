// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import QtQml.Models
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.ScrollViewKCM {
    id: page

    KCM.ConfigModule.buttons: KCM.ConfigModule.NoAdditionalButton
    implicitWidth: Kirigami.Units.gridUnit * 42
    implicitHeight: Kirigami.Units.gridUnit * 32

    function penDescription() {
        if (!kcm.accessoryKnown)
            return i18nd("plasma_applet_org.senemos.nabu.flashlight", "Checking hardware…")
        if (!kcm.penPaired)
            return i18nd("plasma_applet_org.senemos.nabu.flashlight", "No paired Xiaomi Smart Pen was found")
        const parts = [kcm.penConnected ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Connected") : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Paired")]
        if (kcm.penBattery >= 0)
            parts.push(i18nd("plasma_applet_org.senemos.nabu.flashlight", "Battery: %1%", kcm.penBattery))
        if (kcm.penCharging)
            parts.push(i18nd("plasma_applet_org.senemos.nabu.flashlight", "Charging"))
        if (kcm.penChargeLimit >= 0)
            parts.push(i18nd("plasma_applet_org.senemos.nabu.flashlight", "Charge limit: %1%", kcm.penChargeLimit))
        return parts.join(" · ")
    }

    function sensorValue(value) {
        return value.length ? value : i18nd("plasma_applet_org.senemos.nabu.flashlight", "None")
    }

    header: Kirigami.InlineMessage {
        visible: kcm.errorText.length > 0
        type: Kirigami.MessageType.Error
        text: kcm.errorText
        showCloseButton: false
    }

    view: ListView {
        id: list
        clip: true
        spacing: Kirigami.Units.smallSpacing
        property real delegateWidth: Math.min(width, Kirigami.Units.gridUnit * 56)

        model: ObjectModel {
            Kirigami.Card {
                width: list.delegateWidth
                x: Math.max(0, (list.width - width) / 2)
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Device controls"); level: 2 }

                    SettingRow {
                        title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Flashlight")
                        description: kcm.flashlightEnabled
                            ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "On at %1%", kcm.flashlightBrightness) : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Off")
                        iconName: "flashlight-on"
                        known: kcm.flashlightKnown
                        available: kcm.flashlightAvailable
                        checked: kcm.flashlightEnabled
                        onToggled: checked => kcm.setFlashlightEnabled(checked)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        enabled: kcm.flashlightAvailable
                        Controls.Label { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Brightness"); color: Kirigami.Theme.disabledTextColor }
                        Controls.Slider {
                            id: flashlightSlider
                            Layout.fillWidth: true
                            from: 1
                            to: 100
                            stepSize: 1
                            value: kcm.flashlightBrightness
                            onPressedChanged: if (!pressed) kcm.setFlashlightBrightness(Math.round(value))
                            Accessible.name: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Flashlight brightness")
                        }
                        Controls.Label { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "%1%", Math.round(flashlightSlider.value)) }
                    }
                }
            }

            Kirigami.Card {
                width: list.delegateWidth
                x: Math.max(0, (list.width - width) / 2)
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Display and cover"); level: 2 }

                    SettingRow {
                        title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Automatic rotation")
                        description: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Rotate the internal display from the accelerometer orientation")
                        iconName: "object-rotate-right"
                        available: kcm.autoRotateAvailable
                        checked: kcm.autoRotateEnabled
                        onToggled: checked => kcm.setAutoRotateEnabled(checked)
                    }
                    SettingRow {
                        title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Automatic brightness")
                        description: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Let PowerDevil use the ambient-light sensor")
                        iconName: "brightness-high"
                        available: kcm.autoBrightnessAvailable
                        checked: kcm.autoBrightnessEnabled
                        onToggled: checked => kcm.setAutoBrightnessEnabled(checked)
                    }
                    SettingRow {
                        title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Magnetic cover sleep")
                        description: kcm.coverClosed ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Cover is closed") : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Suspend when the cover closes")
                        iconName: "input-tablet"
                        available: kcm.coverAvailable
                        checked: kcm.coverSleepEnabled
                        onToggled: checked => kcm.setCoverSleepEnabled(checked)
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: childrenRect.height
                        spacing: Kirigami.Units.smallSpacing
                        Controls.Button { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Display Configuration"); icon.name: "preferences-desktop-display"; onClicked: kcm.openSettings("kcm_kscreen") }
                        Controls.Button { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Color Profiles"); icon.name: "preferences-color"; onClicked: kcm.openSettings("kcm_colord") }
                        Controls.Button { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Night Light"); icon.name: "redshift-status-on"; onClicked: kcm.openSettings("kcm_nightlight") }
                        Controls.Button { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Power Management"); icon.name: "preferences-system-power-management"; onClicked: kcm.openSettings("kcm_powerdevilprofilesconfig") }
                    }
                }
            }

            Kirigami.Card {
                width: list.delegateWidth
                x: Math.max(0, (list.width - width) / 2)
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Wake and grip"); level: 2 }
                    SettingRow {
                        title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Double tap to wake")
                        description: kcm.doubleTapAvailable
                            ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Wake the tablet by double-tapping the sleeping touchscreen")
                            : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Requires the Nabu touchscreen runtime-control ABI")
                        iconName: "input-touchscreen"
                        known: kcm.doubleTapKnown
                        available: kcm.doubleTapAvailable
                        checked: kcm.doubleTapEnabled
                        onToggled: checked => kcm.setDoubleTapEnabled(checked)
                    }
                    SettingRow {
                        title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Tilt to wake")
                        description: kcm.tiltWakeAvailable
                            ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Use the low-power Sensor DSP tilt event; %1 reports observed", kcm.tiltWakeReports)
                            : i18nd("plasma_applet_org.senemos.nabu.flashlight", "The Sensor DSP tilt endpoint or wake input bridge is unavailable")
                        iconName: "object-rotate-right"
                        known: kcm.tiltWakeKnown
                        available: kcm.tiltWakeAvailable
                        checked: kcm.tiltWakeEnabled
                        onToggled: checked => kcm.setTiltWakeEnabled(checked)
                    }
                    SettingRow {
                        title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Keep awake while held")
                        description: kcm.gripMappingEnabled
                            ? (kcm.gripDataUsable
                                ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Grip state: %1", kcm.gripState)
                                : kcm.gripQualityText)
                            : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Disabled until controlled SAR channel calibration is complete")
                        iconName: "system-suspend"
                        available: kcm.gripAvailable && kcm.gripMappingEnabled
                            && (kcm.gripDataUsable || kcm.gripHoldAwakeEnabled)
                        checked: kcm.gripHoldAwakeEnabled
                        onToggled: checked => kcm.setGripHoldAwakeEnabled(checked)
                    }
                }
            }

            Kirigami.Card {
                width: list.delegateWidth
                x: Math.max(0, (list.width - width) / 2)
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Pen and keyboard"); level: 2 }
                    InfoRow {
                        title: kcm.penName.length && kcm.penName !== "unknown" ? kcm.penName : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Xiaomi Smart Pen")
                        value: page.penDescription()
                    }
                    InfoRow {
                        title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Pogo keyboard")
                        value: kcm.accessoryKnown
                            ? (kcm.keyboardAttached ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Attached") : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Not attached"))
                            : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Checking hardware…")
                    }
                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: childrenRect.height
                        spacing: Kirigami.Units.smallSpacing
                        Controls.Button {
                            visible: kcm.penPaired && !kcm.penConnected
                            enabled: !kcm.busy
                            text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Connect Pen")
                            icon.name: "network-bluetooth"
                            onClicked: kcm.connectPen()
                        }
                        Controls.Button { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Pen Calibration"); icon.name: "input-tablet"; onClicked: kcm.openSettings("kcm_tablet") }
                        Controls.Button { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Bluetooth Devices"); icon.name: "preferences-system-bluetooth"; onClicked: kcm.openSettings("kcm_bluetooth") }
                        Controls.Button { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Keyboard Settings"); icon.name: "input-keyboard"; onClicked: kcm.openSettings("kcm_keyboard") }
                    }
                    Controls.Label {
                        Layout.fillWidth: true
                        text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Pen pressure, button mapping, handedness and calibration remain in KDE's native Drawing Tablet page. Keyboard layout and repeat behavior remain in Keyboard settings.")
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Kirigami.Card {
                width: list.delegateWidth
                x: Math.max(0, (list.width - width) / 2)
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "USB-C policy"); level: 2 }
                    SettingRow {
                        title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "USB device sharing")
                        description: kcm.usbGadgetState === "active"
                            ? kcm.usbServiceSummary : i18nd("plasma_applet_org.senemos.nabu.flashlight", "MTP, optional ADB, network, serial and SSH sharing are inactive")
                        iconName: "network-wired"
                        checked: kcm.usbGadgetState === "active"
                        onToggled: checked => kcm.setUsbSharingEnabled(checked)
                    }
                    Controls.ComboBox {
                        Layout.fillWidth: true
                        model: [i18nd("plasma_applet_org.senemos.nabu.flashlight", "Off"), i18nd("plasma_applet_org.senemos.nabu.flashlight", "Host"), i18nd("plasma_applet_org.senemos.nabu.flashlight", "USB sharing")]
                        currentIndex: kcm.usbMode === "host" ? 1 : (kcm.usbMode === "device" ? 2 : 0)
                        onActivated: index => kcm.setUsbMode(["off", "host", "gadget"][index])
                        Accessible.name: i18nd("plasma_applet_org.senemos.nabu.flashlight", "USB data mode")
                    }
                    Controls.ComboBox {
                        Layout.fillWidth: true
                        model: [i18nd("plasma_applet_org.senemos.nabu.flashlight", "Automatic power role"), i18nd("plasma_applet_org.senemos.nabu.flashlight", "Supply power"), i18nd("plasma_applet_org.senemos.nabu.flashlight", "Receive power")]
                        currentIndex: kcm.usbPowerRole === "source" ? 1 : (kcm.usbPowerRole === "sink" ? 2 : 0)
                        onActivated: index => kcm.setUsbPowerRole(["dual", "source", "sink"][index])
                        Accessible.name: i18nd("plasma_applet_org.senemos.nabu.flashlight", "USB power role")
                    }
                    Kirigami.InlineMessage {
                        Layout.fillWidth: true
                        visible: true
                        type: Kirigami.MessageType.Warning
                        text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Changing USB data or power role can disconnect the current USB session. Network and SSH sharing follow the existing administrator policy.")
                    }
                }
            }

            Kirigami.Card {
                width: list.delegateWidth
                x: Math.max(0, (list.width - width) / 2)
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        Kirigami.Heading { Layout.fillWidth: true; text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Live sensors"); level: 2 }
                        Controls.Button {
                            text: kcm.sensorLive ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Stop") : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Start")
                            icon.name: kcm.sensorLive ? "media-playback-stop" : "media-playback-start"
                            onClicked: kcm.sensorLive ? kcm.stopSensorLive() : kcm.startSensorLive()
                        }
                    }

                    Kirigami.InlineMessage {
                        Layout.fillWidth: true
                        visible: true
                        type: kcm.sensorLive ? Kirigami.MessageType.Positive : Kirigami.MessageType.Information
                        text: kcm.sensorLive
                            ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Live view is active. Updates arrive from D-Bus events without polling.")
                            : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Live view is stopped. No sensor refresh timer or background command is running.")
                    }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Sensor DSP"); value: kcm.sensorSummary }
                    Kirigami.InlineMessage {
                        Layout.fillWidth: true
                        visible: kcm.gripAvailable
                        type: kcm.gripDataUsable ? Kirigami.MessageType.Positive : Kirigami.MessageType.Warning
                        text: kcm.gripQualityText
                    }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "ADUX1050 data quality"); value: kcm.gripSampleQuality }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Identical reports"); value: String(kcm.gripIdenticalSamples) }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Saturated channel mask"); value: "0x" + kcm.gripSaturatedChannelMask.toString(16) }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "ADUX1050 delta CH0, CH1, CH2"); value: page.sensorValue(kcm.gripChannels) }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "ADUX1050 raw CH0, CH1, CH2"); value: page.sensorValue(kcm.gripRawValues) }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "ADUX1050 baseline CH0, CH1, CH2"); value: page.sensorValue(kcm.gripBaselines) }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Firmware endpoints"); value: page.sensorValue(kcm.sensorAvailable) }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Safely monitored"); value: page.sensorValue(kcm.sensorMonitoring) }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Reports observed"); value: page.sensorValue(kcm.sensorReports) }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Fresh reports"); value: page.sensorValue(kcm.sensorFresh) }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Discovered without continuous monitoring"); value: page.sensorValue(kcm.sensorDiscoveredOnly) }
                }
            }

            Kirigami.Card {
                width: list.delegateWidth
                x: Math.max(0, (list.width - width) / 2)
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Grip calibration"); level: 2 }
                    Kirigami.InlineMessage {
                        Layout.fillWidth: true
                        visible: true
                        type: kcm.calibrationReady ? Kirigami.MessageType.Positive : Kirigami.MessageType.Information
                        text: kcm.calibrationMessage
                    }
                    Controls.Label {
                        Layout.fillWidth: true
                        text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Select only the edges you can test. The current Nabu layout usually uses CH0 and CH2; CH1 remains available for hardware variants.")
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        enabled: kcm.calibrationPhase === "idle"
                        Controls.CheckBox { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "CH0"); checked: Boolean(kcm.calibrationChannelMask & 1); onToggled: kcm.setCalibrationChannelEnabled(0, checked) }
                        Controls.CheckBox { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "CH1"); checked: Boolean(kcm.calibrationChannelMask & 2); onToggled: kcm.setCalibrationChannelEnabled(1, checked) }
                        Controls.CheckBox { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "CH2"); checked: Boolean(kcm.calibrationChannelMask & 4); onToggled: kcm.setCalibrationChannelEnabled(2, checked) }
                        Item { Layout.fillWidth: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Controls.Button {
                            text: kcm.calibrationPhase === "released" ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Stop Released Capture") : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Capture Released")
                            enabled: kcm.gripAvailable && kcm.gripDataUsable && kcm.calibrationChannelMask !== 0
                                && (kcm.calibrationPhase === "idle" || kcm.calibrationPhase === "released")
                            onClicked: kcm.calibrationPhase === "released"
                                ? kcm.stopCalibrationCapture() : kcm.startCalibrationCapture("released")
                        }
                        Controls.Label { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "%1/10 samples", kcm.calibrationReleasedSamples); color: Kirigami.Theme.disabledTextColor }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Controls.Button {
                            text: kcm.calibrationPhase === "held" ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Stop Held Capture") : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Capture Held")
                            enabled: kcm.gripAvailable && kcm.gripDataUsable && kcm.calibrationChannelMask !== 0
                                && (kcm.calibrationPhase === "idle" || kcm.calibrationPhase === "held")
                            onClicked: kcm.calibrationPhase === "held"
                                ? kcm.stopCalibrationCapture() : kcm.startCalibrationCapture("held")
                        }
                        Controls.Label { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "%1/10 samples", kcm.calibrationHeldSamples); color: Kirigami.Theme.disabledTextColor }
                    }
                    InfoRow {
                        title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Proposed Linux thresholds")
                        value: kcm.calibrationReady
                            ? i18nd("plasma_applet_org.senemos.nabu.flashlight", "Held ≥ %1 · released ≤ %2 · debounce 3", Math.round(kcm.proposedHeldThreshold), Math.round(kcm.proposedReleasedThreshold))
                            : i18nd("plasma_applet_org.senemos.nabu.flashlight", "Not ready")
                    }
                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: childrenRect.height
                        spacing: Kirigami.Units.smallSpacing
                        Controls.Button {
                            text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Apply Calibration")
                            icon.name: "dialog-ok-apply"
                            enabled: kcm.calibrationReady && !kcm.busy
                            onClicked: kcm.applySarCalibration()
                        }
                        Controls.Button { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Clear Capture"); icon.name: "edit-clear"; enabled: !kcm.busy; onClicked: kcm.clearCalibration() }
                        Controls.Button {
                            text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Disable Grip Mapping")
                            icon.name: "dialog-cancel"
                            enabled: kcm.gripMappingEnabled && !kcm.busy
                            onClicked: kcm.disableSarCalibration()
                        }
                    }
                    Controls.Label {
                        Layout.fillWidth: true
                        text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Calibration changes only /etc/nabu-sar.conf after administrator authentication. Sensor firmware, Android partitions and factory calibration storage stay read-only.")
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Kirigami.Card {
                width: list.delegateWidth
                x: Math.max(0, (list.width - width) / 2)
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing
                    RowLayout {
                        Layout.fillWidth: true
                        Kirigami.Heading { Layout.fillWidth: true; text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Hardware information"); level: 2 }
                        Controls.Button { text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Refresh"); icon.name: "view-refresh"; enabled: !kcm.busy; onClicked: kcm.refresh() }
                    }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Camera EEPROM calibration"); value: kcm.cameraSummary }
                    InfoRow { title: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Partition and DTBO provenance"); value: kcm.provenanceSummary }
                    Controls.Label {
                        Layout.fillWidth: true
                        text: i18nd("plasma_applet_org.senemos.nabu.flashlight", "Availability means firmware advertised an endpoint. Physical function is confirmed only by an observed data report and a device test. Android firmware, active-slot DTBO, modem identity and camera EEPROM payloads are consumed read-only.")
                        color: Kirigami.Theme.disabledTextColor
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
