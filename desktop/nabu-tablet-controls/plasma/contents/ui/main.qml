import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.plasma5support as Plasma5Support

PlasmoidItem {
    id: root

    property bool torchOn: false
    property int brightness: Plasmoid.configuration.brightness || 13
    property bool autoRotate: false
    property bool autoBrightness: false
    property bool lidAction: false
    property bool lidClosed: false
    property string usbDataRole: "unknown"
    property string usbPowerRole: "unknown"
    property string usbGadgetState: "inactive"
    property string usbMtpState: "unavailable"
    property string usbAdbState: "unavailable"
    property string usbNetworkState: "unavailable"
    property string usbSerialState: "unavailable"
    property string usbSshState: "unavailable"
    property bool penPaired: false
    property bool penConnected: false
    property string penAddress: ""
    property string penName: ""
    property int penBattery: -1
    property bool penCharging: false
    property int penChargeLimit: -1
    property bool keyboardAttached: false
    property bool sarMappingEnabled: false
    property bool sarHoldAwake: false
    property bool sarSleepInhibited: false
    property string sarGripState: "unknown"

    property bool flashKnown: false
    property bool flashAvailable: false
    property bool displayKnown: false
    property bool autoRotateAvailable: false
    property bool autoBrightnessAvailable: false
    property bool lidKnown: false
    property bool lidAvailable: false
    property bool usbKnown: false
    property bool usbAvailable: false
    property bool accessoryKnown: false
    property bool sarKnown: false
    property bool sarAvailable: false

    property bool torchBusy: false
    property bool rotateBusy: false
    property bool brightnessBusy: false
    property bool lidBusy: false
    property bool usbGadgetBusy: false
    property bool usbRoleBusy: false
    property bool usbPowerBusy: false
    property bool penBusy: false
    property bool sarBusy: false
    property bool torchLevelInFlight: false
    property bool torchLevelPending: false
    property string errorText: ""
    property int requestSerial: 0
    property var requestKinds: ({})

    function execute(command, kind) {
        const source = "env NABU_WIDGET_REQUEST=" + (++requestSerial) + " " + command
        requestKinds[source] = kind
        executable.connectSource(source)
    }

    function refreshFlash() {
        execute("/usr/bin/nabu-flashlightctl status", "flash-status")
    }

    function refreshDisplay() {
        execute("LANG=C kscreen-doctor -o", "display-status")
    }

    function refreshLid() {
        execute("busctl --user call org.kde.Solid.PowerManagement /org/kde/Solid/PowerManagement org.kde.Solid.PowerManagement isLidPresent", "lid-present")
        execute("busctl --user call org.kde.Solid.PowerManagement /org/kde/Solid/PowerManagement org.kde.Solid.PowerManagement isLidClosed", "lid-closed")
        execute("busctl --user call org.kde.Solid.PowerManagement /org/kde/Solid/PowerManagement/Actions/HandleButtonEvents org.kde.Solid.PowerManagement.Actions.HandleButtonEvents triggersLidAction", "lid-action-status")
    }

    function refreshUsb() {
        execute("/usr/bin/nabu-usb-role status", "usb-role-status")
        execute("/usr/bin/nabu-usb-gadget status", "usb-gadget-status")
    }

    function refreshAccessories() {
        execute("/usr/bin/nabu-accessory-state status", "accessory-status")
    }

    function refreshSar() {
        execute("/usr/libexec/nabu-sar-control status", "sar-status")
    }

    function refreshAll() {
        refreshFlash()
        refreshDisplay()
        refreshLid()
        refreshUsb()
        refreshAccessories()
        refreshSar()
    }

    function statusValue(output, key, fallback) {
        const match = output.match(new RegExp("(?:^|\\n)" + key + "=([^\\n]+)"))
        return match ? match[1].trim() : fallback
    }

    function selectedRole(output, key) {
        const match = output.match(new RegExp(key + "=[^\\n]*\\[([^\\]]+)\\]"))
        return match ? match[1] : "unknown"
    }

    function updateTorch(output) {
        torchOn = output.startsWith("on")
        const fields = output.split(/\s+/)
        if (torchOn && fields.length > 1 && !isNaN(Number(fields[1])))
            brightness = Math.max(1, Math.min(100, Number(fields[1])))
    }

    function finishAction(kind, succeeded) {
        if (kind === "torch-action") {
            torchBusy = false
            if (!succeeded)
                refreshFlash()
        } else if (kind === "rotate-action") {
            rotateBusy = false
            if (!succeeded)
                autoRotate = !autoRotate
        } else if (kind === "brightness-action") {
            brightnessBusy = false
            if (!succeeded)
                autoBrightness = !autoBrightness
        } else if (kind === "lid-action") {
            lidBusy = false
            if (!succeeded)
                lidAction = !lidAction
        } else if (kind === "usb-gadget-action") {
            usbGadgetBusy = false
            refreshUsb()
        } else if (kind === "usb-role-action") {
            usbRoleBusy = false
            refreshUsb()
        } else if (kind === "usb-power-action") {
            usbPowerBusy = false
            refreshUsb()
        } else if (kind === "pen-connect-action") {
            penBusy = false
            refreshAccessories()
        } else if (kind === "sar-action") {
            sarBusy = false
            refreshSar()
        }
    }

    function applyResult(source, data) {
        const kind = requestKinds[source] || ""
        delete requestKinds[source]
        const output = String(data.stdout || "").replace(/\u001b\[[0-9;]*m/g, "").trim()
        const succeeded = data["exit code"] === 0

        if (kind.endsWith("-action")) {
            finishAction(kind, succeeded)
            if (!succeeded)
                errorText = i18n("The requested change could not be applied.")
        }

        if (kind === "torch-level") {
            torchLevelInFlight = false
            if (!succeeded)
                errorText = i18n("The flashlight brightness could not be applied.")
            if (torchLevelPending) {
                torchLevelPending = false
                Qt.callLater(commitTorchLevel)
            }
        } else if (kind === "flash-status" || kind === "torch-action") {
            if (kind === "flash-status") {
                flashKnown = true
                flashAvailable = succeeded
            }
            if (succeeded)
                updateTorch(output)
        } else if (kind === "display-status") {
            displayKnown = true
            autoRotateAvailable = false
            autoBrightnessAvailable = false
            if (succeeded) {
                const panelMatch = output.match(/(?:^|\n)Output:\s+\d+\s+DSI-1\b/)
                if (panelMatch) {
                    const panelStart = panelMatch.index
                    const nextOutput = output.indexOf("\nOutput:", panelStart + panelMatch[0].length)
                    const panel = output.slice(panelStart, nextOutput < 0 ? output.length : nextOutput)
                    const rotation = panel.match(/Auto Rotate Policy:\s*(\S+)/)
                    const automatic = panel.match(/Automatic brightness:\s*([^\n]+)/)
                    autoRotateAvailable = Boolean(rotation && rotation[1] !== "unsupported")
                    autoBrightnessAvailable = Boolean(automatic && !automatic[1].startsWith("unsupported"))
                    if (autoRotateAvailable)
                        autoRotate = rotation[1] === "always"
                    if (autoBrightnessAvailable)
                        autoBrightness = automatic[1].includes("enabled")
                }
            }
        } else if (kind === "lid-present") {
            lidKnown = true
            lidAvailable = succeeded && output.endsWith("true")
        } else if (kind === "lid-closed" && succeeded) {
            lidClosed = output.endsWith("true")
        } else if (kind === "lid-action-status" && succeeded) {
            lidAction = output.endsWith("true")
        } else if (kind === "usb-role-status") {
            usbKnown = true
            usbAvailable = succeeded && output.includes("type=") && output.includes("dual")
            if (succeeded) {
                usbDataRole = selectedRole(output, "data")
                usbPowerRole = selectedRole(output, "power")
            }
        } else if (kind === "usb-gadget-status" && succeeded) {
            usbGadgetState = statusValue(output, "gadget", "inactive")
            usbMtpState = statusValue(output, "mtp", "unavailable")
            usbAdbState = statusValue(output, "adb", "unavailable")
            usbNetworkState = statusValue(output, "network", "unavailable")
            usbSerialState = statusValue(output, "serial", "unavailable")
            usbSshState = statusValue(output, "ssh", "unavailable")
        } else if (kind === "accessory-status") {
            accessoryKnown = succeeded
            penPaired = succeeded && statusValue(output, "pen_paired", "0") === "1"
            penConnected = succeeded && statusValue(output, "pen_connected", "0") === "1"
            penAddress = succeeded ? statusValue(output, "pen_address", "") : ""
            penName = succeeded ? statusValue(output, "pen_name", "") : ""
            penBattery = succeeded ? Number(statusValue(output, "pen_battery", "-1")) : -1
            penCharging = succeeded && statusValue(output, "pen_charging", "0") === "1"
            penChargeLimit = succeeded ? Number(statusValue(output, "pen_charge_limit", "-1")) : -1
            keyboardAttached = succeeded && statusValue(output, "keyboard_attached", "0") === "1"
        } else if (kind === "sar-status" || kind === "sar-action") {
            sarKnown = true
            const sensorAvailable = succeeded && statusValue(output, "available", "0") === "1"
            sarMappingEnabled = succeeded && statusValue(output, "mapping_enabled", "0") === "1"
            sarAvailable = sensorAvailable
            sarGripState = succeeded ? statusValue(output, "grip_state", "unknown") : "unknown"
            sarHoldAwake = succeeded && statusValue(output, "hold_awake_enabled", "0") === "1"
            sarSleepInhibited = succeeded && statusValue(output, "sleep_inhibited", "0") === "1"
        }
    }

    function setTorch(enabled) {
        if (torchBusy)
            return
        errorText = ""
        torchOn = enabled
        torchBusy = true
        execute("/usr/bin/nabu-flashlightctl " + (enabled ? "on " + brightness : "off"), "torch-action")
    }

    function commitTorchLevel() {
        if (!torchOn || !flashAvailable)
            return
        if (torchLevelInFlight) {
            torchLevelPending = true
            return
        }
        torchLevelInFlight = true
        execute("/usr/bin/nabu-flashlightctl set " + brightness, "torch-level")
    }

    function setAutoRotate(enabled) {
        autoRotate = enabled
        rotateBusy = true
        execute("kscreen-doctor output.DSI-1.autoRotatePolicy." + (enabled ? "always" : "never"), "rotate-action")
    }

    function setAutoBrightness(enabled) {
        autoBrightness = enabled
        brightnessBusy = true
        execute("kscreen-doctor output.DSI-1.autoBrightness." + (enabled ? "enable" : "disable"), "brightness-action")
    }

    function setLidAction(enabled) {
        lidAction = enabled
        lidBusy = true
        const action = enabled ? "32" : "0"
        execute("kwriteconfig6 --file powerdevilrc --group AC --group SuspendAndShutdown --key LidAction " + action
            + " && kwriteconfig6 --file powerdevilrc --group Battery --group SuspendAndShutdown --key LidAction " + action
            + " && kwriteconfig6 --file powerdevilrc --group LowBattery --group SuspendAndShutdown --key LidAction " + action
            + " && busctl --user call org.kde.Solid.PowerManagement /org/kde/Solid/PowerManagement org.kde.Solid.PowerManagement reparseConfiguration", "lid-action")
    }

    function setUsbSharing(enabled) {
        usbGadgetState = enabled ? "active" : "inactive"
        usbGadgetBusy = true
        execute("pkexec /usr/libexec/nabu-usb-role set mode " + (enabled ? "gadget" : "off"), "usb-gadget-action")
    }

    function setUsbDataRole(role) {
        usbDataRole = role
        usbRoleBusy = true
        execute("pkexec /usr/libexec/nabu-usb-role set mode " + (role === "host" ? "host" : "gadget"), "usb-role-action")
    }

    function setUsbPowerRole(role) {
        usbPowerRole = role
        usbPowerBusy = true
        execute("pkexec /usr/libexec/nabu-usb-role set power " + role, "usb-power-action")
    }

    function penDescription() {
        const parts = [penConnected ? i18n("Connected") : i18n("Paired")]
        if (penBattery >= 0)
            parts.push(i18n("Battery: %1%", penBattery))
        if (penCharging)
            parts.push(i18n("Charging"))
        if (penChargeLimit >= 0)
            parts.push(i18n("Charge limit: %1%", penChargeLimit))
        return parts.join(" · ")
    }

    function connectPen() {
        if (penBusy || penConnected || !/^([0-9A-F]{2}:){5}[0-9A-F]{2}$/i.test(penAddress))
            return
        errorText = ""
        penBusy = true
        execute("/usr/bin/nabu-accessory-state connect " + penAddress, "pen-connect-action")
    }

    function setSarHoldAwake(enabled) {
        if (sarBusy || !sarAvailable || !sarMappingEnabled)
            return
        errorText = ""
        sarHoldAwake = enabled
        sarBusy = true
        execute("pkexec /usr/libexec/nabu-sar-control set hold-awake "
            + (enabled ? "on" : "off"), "sar-action")
    }

    function sarDescription() {
        if (!sarMappingEnabled)
            return i18n("Calibration required")
        if (!sarHoldAwake)
            return i18n("Inactive")
        if (sarSleepInhibited)
            return i18n("Held · sleep blocked")
        if (sarGripState === "held")
            return i18n("Held · acquiring inhibitor")
        if (sarGripState === "released")
            return i18n("Waiting until held")
        return i18n("Waiting for sensor data")
    }

    toolTipMainText: i18n("Tablet Control")
    toolTipSubText: torchOn ? i18n("Flashlight on") : i18n("Nabu hardware controls")
    Plasmoid.icon: torchOn ? "flashlight-on" : "input-tablet"
    Plasmoid.status: torchOn ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus

    Plasma5Support.DataSource {
        id: executable
        engine: "executable"
        onNewData: function(source, data) {
            root.applyResult(source, data)
            disconnectSource(source)
        }
    }

    Timer {
        id: torchCommitTimer
        interval: 40
        repeat: false
        onTriggered: root.commitTorchLevel()
    }

    Component.onCompleted: refreshFlash()
    onExpandedChanged: function(expanded) {
        if (expanded)
            refreshAll()
    }

    compactRepresentation: PlasmaComponents.ToolButton {
        icon.name: root.torchOn ? "flashlight-on" : "input-tablet"
        onClicked: root.expanded = !root.expanded
        Accessible.name: root.toolTipMainText
    }

    fullRepresentation: PlasmaComponents.ScrollView {
        id: scrollView

        Layout.minimumWidth: Kirigami.Units.gridUnit * 22
        Layout.preferredWidth: Kirigami.Units.gridUnit * 26
        Layout.maximumWidth: Kirigami.Units.gridUnit * 34
        Layout.minimumHeight: Kirigami.Units.gridUnit * 18
        Layout.preferredHeight: Kirigami.Units.gridUnit * 30
        Layout.maximumHeight: Kirigami.Units.gridUnit * 36
        contentWidth: availableWidth
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff
        QQC2.ScrollBar.vertical.policy: QQC2.ScrollBar.AsNeeded

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing

                Kirigami.Heading {
                    Layout.fillWidth: true
                    text: i18n("Tablet Control")
                    level: 2
                }

                PlasmaComponents.ToolButton {
                    icon.name: "view-refresh"
                    text: i18n("Refresh hardware state")
                    display: PlasmaComponents.AbstractButton.IconOnly
                    onClicked: root.refreshAll()
                    PlasmaComponents.ToolTip.text: text
                    PlasmaComponents.ToolTip.visible: hovered
                }
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.rightMargin: Kirigami.Units.largeSpacing
                Layout.bottomMargin: visible ? Kirigami.Units.smallSpacing : 0
                type: Kirigami.MessageType.Error
                text: root.errorText
                visible: text.length > 0
                showCloseButton: true
                onLinkActivated: root.errorText = ""
            }

            SectionHeader { text: i18n("Device") }

            ControlRow {
                title: i18n("Flashlight")
                description: root.torchOn ? i18n("On at %1%", root.brightness) : i18n("Off")
                iconName: "flashlight-on"
                capabilityKnown: root.flashKnown
                available: root.flashAvailable
                busy: root.torchBusy
                action: Component {
                    PlasmaComponents.Switch {
                        checked: root.torchOn
                        onToggled: root.setTorch(checked)
                        Accessible.name: i18n("Flashlight")
                    }
                }
                details: Component {
                    RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "brightness-high"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: implicitWidth
                        }

                        PlasmaComponents.Slider {
                            id: torchSlider
                            Layout.fillWidth: true
                            from: 1
                            to: 100
                            stepSize: 1
                            value: root.brightness
                            onMoved: {
                                root.brightness = Math.round(value)
                                torchCommitTimer.restart()
                            }
                            onPressedChanged: {
                                if (!pressed) {
                                    torchCommitTimer.stop()
                                    Plasmoid.configuration.brightness = root.brightness
                                    root.commitTorchLevel()
                                }
                            }
                            Accessible.name: i18n("Flashlight brightness")
                            Accessible.description: i18n("%1 percent", root.brightness)
                        }

                        PlasmaComponents.Label {
                            Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                            horizontalAlignment: Text.AlignRight
                            text: i18n("%1%", root.brightness)
                        }
                    }
                }
            }

            SectionHeader { text: i18n("Display and cover") }

            ControlRow {
                title: i18n("Automatic rotation")
                description: root.autoRotate ? i18n("Active") : i18n("Inactive")
                iconName: "object-rotate-right"
                capabilityKnown: root.displayKnown
                available: root.autoRotateAvailable
                busy: root.rotateBusy
                action: Component {
                    PlasmaComponents.Switch {
                        checked: root.autoRotate
                        onToggled: root.setAutoRotate(checked)
                        Accessible.name: i18n("Automatic rotation")
                    }
                }
            }

            ControlRow {
                title: i18n("Automatic brightness")
                description: root.autoBrightness ? i18n("Active") : i18n("Inactive")
                iconName: "brightness-high"
                capabilityKnown: root.displayKnown
                available: root.autoBrightnessAvailable
                busy: root.brightnessBusy
                action: Component {
                    PlasmaComponents.Switch {
                        checked: root.autoBrightness
                        onToggled: root.setAutoBrightness(checked)
                        Accessible.name: i18n("Automatic brightness")
                    }
                }
            }

            ControlRow {
                title: i18n("Magnetic cover")
                description: (root.lidClosed ? i18n("Closed") : i18n("Open"))
                    + " · " + (root.lidAction ? i18n("Sleep action active") : i18n("Sleep action inactive"))
                iconName: "input-tablet"
                capabilityKnown: root.lidKnown
                available: root.lidAvailable
                busy: root.lidBusy
                action: Component {
                    PlasmaComponents.Switch {
                        checked: root.lidAction
                        onToggled: root.setLidAction(checked)
                        Accessible.name: i18n("Magnetic cover sleep action")
                    }
                }
            }

            ControlRow {
                title: i18n("Keep awake while held")
                description: root.sarDescription()
                iconName: "system-suspend"
                capabilityKnown: root.sarKnown
                available: root.sarAvailable
                controlEnabled: root.sarMappingEnabled
                busy: root.sarBusy
                action: Component {
                    PlasmaComponents.Switch {
                        checked: root.sarHoldAwake
                        onToggled: root.setSarHoldAwake(checked)
                        Accessible.name: i18n("Keep awake while held")
                        Accessible.description: root.sarDescription()
                    }
                }
            }

            SectionHeader {
                text: i18n("Pad 5 Specific")
                visible: root.accessoryKnown && (root.penPaired || root.keyboardAttached)
            }

            ControlRow {
                visible: root.accessoryKnown && root.penPaired
                title: root.penName.length > 0 && root.penName !== "unknown"
                    ? root.penName : i18n("Xiaomi Smart Pen")
                description: root.penDescription()
                iconName: "input-tablet"
                busy: root.penBusy
                action: Component {
                    PlasmaComponents.Button {
                        text: root.penConnected ? i18n("Connected") : i18n("Connect")
                        enabled: !root.penConnected && !root.penBusy
                        onClicked: root.connectPen()
                        Accessible.name: text
                    }
                }
                details: root.penBattery >= 0 ? penBatteryDetails : null
            }

            Component {
                id: penBatteryDetails

                RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: root.penCharging ? "battery-charging" : "battery"
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: implicitWidth
                    }

                    PlasmaComponents.ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: root.penBattery
                        Accessible.name: i18n("Pen battery")
                        Accessible.description: i18n("%1 percent", root.penBattery)
                    }

                    PlasmaComponents.Label {
                        Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                        horizontalAlignment: Text.AlignRight
                        text: i18n("%1%", root.penBattery)
                    }
                }
            }

            ControlRow {
                visible: root.accessoryKnown && root.keyboardAttached
                title: i18n("Pogo keyboard")
                description: i18n("Attached")
                iconName: "input-keyboard"
            }

            SectionHeader { text: i18n("USB-C") }

            ControlRow {
                title: i18n("USB device sharing")
                description: root.usbGadgetState === "active"
                    ? i18n("MTP%1 · Serial%2", root.usbSshState === "ready" ? i18n(" · SSH") : "",
                        root.usbAdbState === "ready" ? i18n(" · ADB") : "")
                    : i18n("Inactive")
                iconName: "network-wired"
                capabilityKnown: root.usbKnown
                available: root.usbAvailable
                busy: root.usbGadgetBusy
                action: Component {
                    PlasmaComponents.Switch {
                        checked: root.usbGadgetState === "active"
                        onToggled: root.setUsbSharing(checked)
                        Accessible.name: i18n("USB device sharing")
                    }
                }
            }

            ControlRow {
                title: i18n("USB data role")
                description: root.usbDataRole === "host" ? i18n("Host mode") : i18n("Device mode")
                iconName: "drive-removable-media-usb"
                capabilityKnown: root.usbKnown
                available: root.usbAvailable
                busy: root.usbRoleBusy
                action: Component {
                    PlasmaComponents.Button {
                        text: root.usbDataRole === "host" ? i18n("Use device") : i18n("Use host")
                        onClicked: root.setUsbDataRole(root.usbDataRole === "host" ? "device" : "host")
                    }
                }
            }

            ControlRow {
                title: i18n("USB power role")
                description: root.usbPowerRole === "source" ? i18n("Supplying power") : i18n("Power sink")
                iconName: "battery-charging"
                capabilityKnown: root.usbKnown
                available: root.usbAvailable
                busy: root.usbPowerBusy
                action: Component {
                    PlasmaComponents.Button {
                        text: root.usbPowerRole === "source" ? i18n("Use sink") : i18n("Supply power")
                        onClicked: root.setUsbPowerRole(root.usbPowerRole === "source" ? "sink" : "source")
                    }
                }
            }

            Item { Layout.preferredHeight: Kirigami.Units.largeSpacing }
        }
    }
}
