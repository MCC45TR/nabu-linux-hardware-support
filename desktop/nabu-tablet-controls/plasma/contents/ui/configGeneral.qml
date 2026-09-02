import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

Kirigami.FormLayout {
    property alias cfg_brightness: brightnessSlider.value

    PlasmaComponents.Slider {
        id: brightnessSlider
        Kirigami.FormData.label: i18n("Default brightness:")
        Layout.fillWidth: true
        from: 1
        to: 100
        stepSize: 1
        Accessible.name: i18n("Default flashlight brightness")
        Accessible.description: Math.round(value) + "%"
    }

    PlasmaComponents.Label {
        Kirigami.FormData.label: i18n("Current value:")
        text: Math.round(brightnessSlider.value) + "%"
    }
}
