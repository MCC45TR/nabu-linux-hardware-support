// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 mcc45tr <mcc45tr@gmail.com>

#include <KLocalizedString>
#include <KPluginFactory>
#include <KQuickConfigModule>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace {
constexpr auto kTranslationDomain = "plasma_applet_org.senemos.nabu.flashlight";
constexpr auto kSensorService = "org.senemos.Nabu.Sar";
constexpr auto kSarObject = "/org/senemos/Nabu/Sar";
constexpr auto kSarInterface = "org.senemos.Nabu.Sar1";
constexpr auto kSensorsObject = "/org/senemos/Nabu/Sensors";
constexpr auto kSensorsInterface = "org.senemos.Nabu.Sensors1";
constexpr int kCalibrationSamples = 10;

QString numberList(const QList<double> &values)
{
    QStringList text;
    text.reserve(values.size());
    for (const double value : values)
        text.append(QString::number(value, 'f', std::abs(value) < 100.0 ? 2 : 0));
    return text.join(QLatin1Char(','));
}

QList<double> doubleList(const QVariant &value)
{
    if (value.canConvert<QList<double>>())
        return value.value<QList<double>>();
    QList<double> result;
    const QDBusArgument argument = value.value<QDBusArgument>();
    if (argument.currentType() != QDBusArgument::ArrayType)
        return result;
    argument.beginArray();
    while (!argument.atEnd()) {
        double item = 0.0;
        argument >> item;
        result.append(item);
    }
    argument.endArray();
    return result;
}
}

class NabuSettings final : public KQuickConfigModule
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(bool flashlightKnown READ flashlightKnown NOTIFY stateChanged)
    Q_PROPERTY(bool flashlightAvailable READ flashlightAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool flashlightEnabled READ flashlightEnabled NOTIFY stateChanged)
    Q_PROPERTY(int flashlightBrightness READ flashlightBrightness NOTIFY stateChanged)
    Q_PROPERTY(bool doubleTapKnown READ doubleTapKnown NOTIFY stateChanged)
    Q_PROPERTY(bool doubleTapAvailable READ doubleTapAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool doubleTapEnabled READ doubleTapEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool tiltWakeKnown READ tiltWakeKnown NOTIFY stateChanged)
    Q_PROPERTY(bool tiltWakeAvailable READ tiltWakeAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool tiltWakeEnabled READ tiltWakeEnabled NOTIFY stateChanged)
    Q_PROPERTY(qulonglong tiltWakeReports READ tiltWakeReports NOTIFY stateChanged)
    Q_PROPERTY(bool autoRotateAvailable READ autoRotateAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool autoRotateEnabled READ autoRotateEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool autoBrightnessAvailable READ autoBrightnessAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool autoBrightnessEnabled READ autoBrightnessEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool coverAvailable READ coverAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool coverClosed READ coverClosed NOTIFY stateChanged)
    Q_PROPERTY(bool coverSleepEnabled READ coverSleepEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool gripAvailable READ gripAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool gripMappingEnabled READ gripMappingEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool gripHoldAwakeEnabled READ gripHoldAwakeEnabled NOTIFY stateChanged)
    Q_PROPERTY(QString gripState READ gripState NOTIFY stateChanged)
    Q_PROPERTY(QString gripChannels READ gripChannels NOTIFY stateChanged)
    Q_PROPERTY(QString gripRawValues READ gripRawValues NOTIFY stateChanged)
    Q_PROPERTY(QString gripBaselines READ gripBaselines NOTIFY stateChanged)
    Q_PROPERTY(QString gripSampleQuality READ gripSampleQuality NOTIFY stateChanged)
    Q_PROPERTY(QString gripQualityText READ gripQualityText NOTIFY stateChanged)
    Q_PROPERTY(bool gripDataUsable READ gripDataUsable NOTIFY stateChanged)
    Q_PROPERTY(bool gripDataChanging READ gripDataChanging NOTIFY stateChanged)
    Q_PROPERTY(int gripIdenticalSamples READ gripIdenticalSamples NOTIFY stateChanged)
    Q_PROPERTY(int gripSaturatedChannelMask READ gripSaturatedChannelMask NOTIFY stateChanged)
    Q_PROPERTY(qulonglong gripSampleSequence READ gripSampleSequence NOTIFY stateChanged)
    Q_PROPERTY(bool sensorLive READ sensorLive NOTIFY stateChanged)
    Q_PROPERTY(QString sensorSummary READ sensorSummary NOTIFY stateChanged)
    Q_PROPERTY(QString sensorAvailable READ sensorAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString sensorMonitoring READ sensorMonitoring NOTIFY stateChanged)
    Q_PROPERTY(QString sensorReports READ sensorReports NOTIFY stateChanged)
    Q_PROPERTY(QString sensorFresh READ sensorFresh NOTIFY stateChanged)
    Q_PROPERTY(QString sensorDiscoveredOnly READ sensorDiscoveredOnly NOTIFY stateChanged)
    Q_PROPERTY(QString calibrationPhase READ calibrationPhase NOTIFY stateChanged)
    Q_PROPERTY(int calibrationChannelMask READ calibrationChannelMask NOTIFY stateChanged)
    Q_PROPERTY(int calibrationReleasedSamples READ calibrationReleasedSamples NOTIFY stateChanged)
    Q_PROPERTY(int calibrationHeldSamples READ calibrationHeldSamples NOTIFY stateChanged)
    Q_PROPERTY(bool calibrationReady READ calibrationReady NOTIFY stateChanged)
    Q_PROPERTY(double proposedHeldThreshold READ proposedHeldThreshold NOTIFY stateChanged)
    Q_PROPERTY(double proposedReleasedThreshold READ proposedReleasedThreshold NOTIFY stateChanged)
    Q_PROPERTY(QString calibrationMessage READ calibrationMessage NOTIFY stateChanged)
    Q_PROPERTY(bool accessoryKnown READ accessoryKnown NOTIFY stateChanged)
    Q_PROPERTY(bool penPaired READ penPaired NOTIFY stateChanged)
    Q_PROPERTY(bool penConnected READ penConnected NOTIFY stateChanged)
    Q_PROPERTY(QString penAddress READ penAddress NOTIFY stateChanged)
    Q_PROPERTY(QString penName READ penName NOTIFY stateChanged)
    Q_PROPERTY(int penBattery READ penBattery NOTIFY stateChanged)
    Q_PROPERTY(bool penCharging READ penCharging NOTIFY stateChanged)
    Q_PROPERTY(int penChargeLimit READ penChargeLimit NOTIFY stateChanged)
    Q_PROPERTY(bool keyboardAttached READ keyboardAttached NOTIFY stateChanged)
    Q_PROPERTY(QString usbMode READ usbMode NOTIFY stateChanged)
    Q_PROPERTY(QString usbPowerRole READ usbPowerRole NOTIFY stateChanged)
    Q_PROPERTY(QString usbGadgetState READ usbGadgetState NOTIFY stateChanged)
    Q_PROPERTY(QString usbServiceSummary READ usbServiceSummary NOTIFY stateChanged)
    Q_PROPERTY(QString cameraSummary READ cameraSummary NOTIFY stateChanged)
    Q_PROPERTY(QString provenanceSummary READ provenanceSummary NOTIFY stateChanged)

public:
    explicit NabuSettings(QObject *parent, const KPluginMetaData &data)
        : KQuickConfigModule(parent, data)
    {
        KLocalizedString::setApplicationDomain(kTranslationDomain);
        // Keep KPlugin metadata in the same catalog as the KCM and widget.
        (void)i18n("Nabu Tablet");
        (void)i18n("Configure Xiaomi Pad 5 hardware integration");
        setButtons(NoAdditionalButton);
        m_calibrationMessage = i18n("Collect released and held samples. Nothing is written until Apply is selected.");
        m_calibrationTimer.setSingleShot(true);
        m_calibrationTimer.setInterval(20000);
        connect(&m_calibrationTimer, &QTimer::timeout, this, [this] {
            if (m_calibrationPhase == QLatin1String("idle"))
                return;
            m_calibrationPhase = QStringLiteral("idle");
            m_calibrationMessage = i18n("Capture timed out. Keep the tablet steady and try that phase again.");
            updateCalibrationProposal();
            Q_EMIT stateChanged();
        });
        QTimer::singleShot(0, this, &NabuSettings::refresh);
    }

    ~NabuSettings() override { stopSensorLive(); }

    bool busy() const { return m_pending > 0; }
    QString errorText() const { return m_errorText; }
    bool flashlightKnown() const { return m_flashlightKnown; }
    bool flashlightAvailable() const { return m_flashlightAvailable; }
    bool flashlightEnabled() const { return m_flashlightEnabled; }
    int flashlightBrightness() const { return m_flashlightBrightness; }
    bool doubleTapKnown() const { return m_doubleTapKnown; }
    bool doubleTapAvailable() const { return m_doubleTapAvailable; }
    bool doubleTapEnabled() const { return m_doubleTapEnabled; }
    bool tiltWakeKnown() const { return m_tiltWakeKnown; }
    bool tiltWakeAvailable() const { return m_tiltWakeAvailable; }
    bool tiltWakeEnabled() const { return m_tiltWakeEnabled; }
    qulonglong tiltWakeReports() const { return m_tiltWakeReports; }
    bool autoRotateAvailable() const { return m_autoRotateAvailable; }
    bool autoRotateEnabled() const { return m_autoRotateEnabled; }
    bool autoBrightnessAvailable() const { return m_autoBrightnessAvailable; }
    bool autoBrightnessEnabled() const { return m_autoBrightnessEnabled; }
    bool coverAvailable() const { return m_coverAvailable; }
    bool coverClosed() const { return m_coverClosed; }
    bool coverSleepEnabled() const { return m_coverSleepEnabled; }
    bool gripAvailable() const { return m_gripAvailable; }
    bool gripMappingEnabled() const { return m_gripMappingEnabled; }
    bool gripHoldAwakeEnabled() const { return m_gripHoldAwakeEnabled; }
    QString gripState() const { return m_gripState; }
    QString gripChannels() const { return m_gripChannels; }
    QString gripRawValues() const { return m_gripRawValues; }
    QString gripBaselines() const { return m_gripBaselines; }
    QString gripSampleQuality() const { return m_gripSampleQuality; }
    QString gripQualityText() const
    {
        if (m_gripSampleQuality == QLatin1String("valid-changing"))
            return i18n("Data is changing and usable");
        if (m_gripSampleQuality == QLatin1String("invalid-saturated"))
            return i18n("One or more channels are saturated; grip actions and calibration are disabled");
        if (m_gripSampleQuality == QLatin1String("stuck-saturated"))
            return i18n("Data is fixed and saturated; grip actions and calibration are disabled");
        if (m_gripSampleQuality == QLatin1String("stuck-constant"))
            return i18n("Reports arrive but values do not change; grip actions and calibration are disabled");
        if (m_gripSampleQuality == QLatin1String("transport-stale"))
            return i18n("The ADUX1050 report stream is stale");
        return i18n("Validating ADUX1050 data variation…");
    }
    bool gripDataUsable() const { return m_gripDataUsable; }
    bool gripDataChanging() const { return m_gripDataChanging; }
    int gripIdenticalSamples() const { return m_gripIdenticalSamples; }
    int gripSaturatedChannelMask() const { return m_gripSaturatedChannelMask; }
    qulonglong gripSampleSequence() const { return m_gripSampleSequence; }
    bool sensorLive() const { return m_sensorLive; }
    QString sensorSummary() const { return m_sensorSummary; }
    QString sensorAvailable() const { return m_sensorAvailable; }
    QString sensorMonitoring() const { return m_sensorMonitoring; }
    QString sensorReports() const { return m_sensorReports; }
    QString sensorFresh() const { return m_sensorFresh; }
    QString sensorDiscoveredOnly() const { return m_sensorDiscoveredOnly; }
    QString calibrationPhase() const { return m_calibrationPhase; }
    int calibrationChannelMask() const { return m_calibrationChannelMask; }
    int calibrationReleasedSamples() const { return m_calibrationReleased.size(); }
    int calibrationHeldSamples() const { return m_calibrationHeld.size(); }
    bool calibrationReady() const { return m_calibrationReady; }
    double proposedHeldThreshold() const { return m_proposedHeldThreshold; }
    double proposedReleasedThreshold() const { return m_proposedReleasedThreshold; }
    QString calibrationMessage() const { return m_calibrationMessage; }
    bool accessoryKnown() const { return m_accessoryKnown; }
    bool penPaired() const { return m_penPaired; }
    bool penConnected() const { return m_penConnected; }
    QString penAddress() const { return m_penAddress; }
    QString penName() const { return m_penName; }
    int penBattery() const { return m_penBattery; }
    bool penCharging() const { return m_penCharging; }
    int penChargeLimit() const { return m_penChargeLimit; }
    bool keyboardAttached() const { return m_keyboardAttached; }
    QString usbMode() const { return m_usbMode; }
    QString usbPowerRole() const { return m_usbPowerRole; }
    QString usbGadgetState() const { return m_usbGadgetState; }
    QString usbServiceSummary() const { return m_usbServiceSummary; }
    QString cameraSummary() const { return m_cameraSummary; }
    QString provenanceSummary() const { return m_provenanceSummary; }

    Q_INVOKABLE void refresh()
    {
        m_errorText.clear();
        run(QStringLiteral("/usr/bin/nabu-flashlightctl"), {QStringLiteral("status")}, QStringLiteral("flash-status"));
        run(QStringLiteral("/usr/libexec/nabu-wake-control"), {QStringLiteral("status")}, QStringLiteral("wake-status"));
        run(QStringLiteral("/usr/bin/kscreen-doctor"), {QStringLiteral("-o")}, QStringLiteral("display-status"));
        run(QStringLiteral("/usr/bin/nabu-usb-role"), {QStringLiteral("status")}, QStringLiteral("usb-status"));
        run(QStringLiteral("/usr/bin/nabu-usb-gadget"), {QStringLiteral("status")}, QStringLiteral("usb-gadget-status"));
        run(QStringLiteral("/usr/bin/nabu-accessory-state"), {QStringLiteral("status")}, QStringLiteral("accessory-status"));
        refreshCover();
        refreshSensorSnapshot();
        refreshReports();
    }

    Q_INVOKABLE void setFlashlightEnabled(bool enabled)
    {
        m_flashlightEnabled = enabled;
        run(QStringLiteral("/usr/bin/nabu-flashlightctl"),
            enabled ? QStringList{QStringLiteral("on"), QString::number(m_flashlightBrightness)}
                    : QStringList{QStringLiteral("off")}, QStringLiteral("action"), true);
    }

    Q_INVOKABLE void setFlashlightBrightness(int brightness)
    {
        if (brightness < 1 || brightness > 100)
            return;
        m_flashlightBrightness = brightness;
        Q_EMIT stateChanged();
        if (m_flashlightEnabled)
            run(QStringLiteral("/usr/bin/nabu-flashlightctl"),
                {QStringLiteral("set"), QString::number(brightness)}, QStringLiteral("action"));
    }

    Q_INVOKABLE void setDoubleTapEnabled(bool enabled)
    {
        m_doubleTapEnabled = enabled;
        privilegedWake(QStringLiteral("double-tap"), enabled);
    }

    Q_INVOKABLE void setTiltWakeEnabled(bool enabled)
    {
        m_tiltWakeEnabled = enabled;
        privilegedWake(QStringLiteral("tilt-wake"), enabled);
    }

    Q_INVOKABLE void setAutoRotateEnabled(bool enabled)
    {
        m_autoRotateEnabled = enabled;
        run(QStringLiteral("/usr/bin/kscreen-doctor"),
            {QStringLiteral("output.DSI-1.autoRotatePolicy.%1").arg(enabled ? QStringLiteral("always") : QStringLiteral("never"))},
            QStringLiteral("action"), true);
    }

    Q_INVOKABLE void setAutoBrightnessEnabled(bool enabled)
    {
        m_autoBrightnessEnabled = enabled;
        run(QStringLiteral("/usr/bin/kscreen-doctor"),
            {QStringLiteral("output.DSI-1.autoBrightness.%1").arg(enabled ? QStringLiteral("enable") : QStringLiteral("disable"))},
            QStringLiteral("action"), true);
    }

    Q_INVOKABLE void setCoverSleepEnabled(bool enabled)
    {
        m_coverSleepEnabled = enabled;
        QSettings settings(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                               + QStringLiteral("/powerdevilrc"), QSettings::IniFormat);
        const int action = enabled ? 32 : 0;
        for (const QString &profile : {QStringLiteral("AC"), QStringLiteral("Battery"), QStringLiteral("LowBattery")})
            settings.setValue(profile + QStringLiteral("/SuspendAndShutdown/LidAction"), action);
        settings.sync();
        QDBusInterface power(QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("/org/kde/Solid/PowerManagement"),
            QStringLiteral("org.kde.Solid.PowerManagement"), QDBusConnection::sessionBus());
        power.asyncCall(QStringLiteral("reparseConfiguration"));
        QTimer::singleShot(250, this, &NabuSettings::refreshCover);
    }

    Q_INVOKABLE void setGripHoldAwakeEnabled(bool enabled)
    {
        if (enabled && (!m_gripMappingEnabled || !m_gripDataUsable))
            return;
        m_gripHoldAwakeEnabled = enabled;
        run(QStringLiteral("/usr/bin/pkexec"),
            {QStringLiteral("/usr/libexec/nabu-sar-control"), QStringLiteral("set"),
             QStringLiteral("hold-awake"), enabled ? QStringLiteral("on") : QStringLiteral("off")},
            QStringLiteral("action"), true);
    }

    Q_INVOKABLE void setUsbMode(const QString &mode)
    {
        static const QStringList allowed{QStringLiteral("off"), QStringLiteral("host"), QStringLiteral("gadget")};
        if (!allowed.contains(mode))
            return;
        run(QStringLiteral("/usr/bin/pkexec"),
            {QStringLiteral("/usr/libexec/nabu-usb-role"), QStringLiteral("set"), QStringLiteral("mode"), mode},
            QStringLiteral("action"), true);
    }

    Q_INVOKABLE void setUsbSharingEnabled(bool enabled)
    {
        setUsbMode(enabled ? QStringLiteral("gadget") : QStringLiteral("off"));
    }

    Q_INVOKABLE void setUsbPowerRole(const QString &role)
    {
        static const QStringList allowed{QStringLiteral("source"), QStringLiteral("sink"), QStringLiteral("dual")};
        if (!allowed.contains(role))
            return;
        run(QStringLiteral("/usr/bin/pkexec"),
            {QStringLiteral("/usr/libexec/nabu-usb-role"), QStringLiteral("set"), QStringLiteral("power"), role},
            QStringLiteral("action"), true);
    }

    Q_INVOKABLE void connectPen()
    {
        static const QRegularExpression address(QStringLiteral("^(?:[0-9A-F]{2}:){5}[0-9A-F]{2}$"));
        if (!m_penPaired || m_penConnected || !address.match(m_penAddress).hasMatch())
            return;
        run(QStringLiteral("/usr/bin/nabu-accessory-state"),
            {QStringLiteral("connect"), m_penAddress}, QStringLiteral("action"), true);
    }

    Q_INVOKABLE void openSettings(const QString &module)
    {
        static const QStringList allowed{
            QStringLiteral("kcm_bluetooth"), QStringLiteral("kcm_colord"), QStringLiteral("kcm_keyboard"),
            QStringLiteral("kcm_kscreen"), QStringLiteral("kcm_nightlight"),
            QStringLiteral("kcm_powerdevilprofilesconfig"), QStringLiteral("kcm_tablet")};
        if (allowed.contains(module))
            QProcess::startDetached(QStringLiteral("/usr/bin/systemsettings"), {module});
    }

    Q_INVOKABLE void startSensorLive()
    {
        if (m_sensorLive)
            return;
        auto bus = QDBusConnection::systemBus();
        const bool sarConnected = bus.connect(QString::fromLatin1(kSensorService), QString::fromLatin1(kSarObject),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
            this, SLOT(onSensorPropertiesChanged(QString,QVariantMap,QStringList)));
        const bool sensorsConnected = bus.connect(QString::fromLatin1(kSensorService), QString::fromLatin1(kSensorsObject),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
            this, SLOT(onSensorPropertiesChanged(QString,QVariantMap,QStringList)));
        if (!sarConnected || !sensorsConnected) {
            bus.disconnect(QString::fromLatin1(kSensorService), QString(),
                QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
                this, SLOT(onSensorPropertiesChanged(QString,QVariantMap,QStringList)));
            m_errorText = i18n("Could not subscribe to the sensor service.");
            Q_EMIT stateChanged();
            return;
        }
        m_sensorLive = true;
        refreshSensorSnapshot();
        Q_EMIT stateChanged();
    }

    Q_INVOKABLE void stopSensorLive()
    {
        if (!m_sensorLive)
            return;
        auto bus = QDBusConnection::systemBus();
        bus.disconnect(QString::fromLatin1(kSensorService), QString(),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
            this, SLOT(onSensorPropertiesChanged(QString,QVariantMap,QStringList)));
        m_sensorLive = false;
        m_calibrationTimer.stop();
        m_calibrationPhase = QStringLiteral("idle");
        Q_EMIT stateChanged();
    }

    Q_INVOKABLE void setCalibrationChannelEnabled(int channel, bool enabled)
    {
        if (channel < 0 || channel > 2 || m_calibrationPhase != QLatin1String("idle"))
            return;
        const int bit = 1 << channel;
        m_calibrationChannelMask = enabled ? (m_calibrationChannelMask | bit) : (m_calibrationChannelMask & ~bit);
        clearCalibration();
    }

    Q_INVOKABLE void startCalibrationCapture(const QString &phase)
    {
        if ((phase != QLatin1String("released") && phase != QLatin1String("held"))
            || !m_gripAvailable || !m_gripDataUsable || m_calibrationChannelMask == 0) {
            if (m_gripAvailable && !m_gripDataUsable) {
                m_calibrationMessage = i18n("Calibration is blocked until ADUX1050 values change without saturation.");
                Q_EMIT stateChanged();
            }
            return;
        }
        if (!m_sensorLive)
            startSensorLive();
        if (!m_sensorLive)
            return;
        if (phase == QLatin1String("released"))
            m_calibrationReleased.clear();
        else
            m_calibrationHeld.clear();
        m_calibrationReady = false;
        m_calibrationPhase = phase;
        m_calibrationMessage = phase == QLatin1String("released")
            ? i18n("Keep hands away from the selected tablet edges while samples are collected.")
            : i18n("Hold the selected tablet edges naturally and keep the grip steady.");
        m_lastCalibrationSequence = 0;
        m_calibrationTimer.start();
        captureCalibrationSample();
        Q_EMIT stateChanged();
    }

    Q_INVOKABLE void stopCalibrationCapture()
    {
        m_calibrationTimer.stop();
        m_calibrationPhase = QStringLiteral("idle");
        updateCalibrationProposal();
        Q_EMIT stateChanged();
    }

    Q_INVOKABLE void clearCalibration()
    {
        m_calibrationTimer.stop();
        m_calibrationPhase = QStringLiteral("idle");
        m_calibrationReleased.clear();
        m_calibrationHeld.clear();
        m_calibrationReady = false;
        m_proposedHeldThreshold = 0.0;
        m_proposedReleasedThreshold = 0.0;
        m_calibrationMessage = i18n("Collect released and held samples. Nothing is written until Apply is selected.");
        Q_EMIT stateChanged();
    }

    Q_INVOKABLE void applySarCalibration()
    {
        if (!m_calibrationReady || m_calibrationChannelMask < 1 || m_calibrationChannelMask > 7)
            return;
        run(QStringLiteral("/usr/bin/pkexec"),
            {QStringLiteral("/usr/libexec/nabu-sar-calibration"), QStringLiteral("apply"),
             QString::number(m_calibrationChannelMask), QString::number(m_proposedHeldThreshold, 'f', 3),
             QString::number(m_proposedReleasedThreshold, 'f', 3), QStringLiteral("3")},
            QStringLiteral("calibration-action"), true);
    }

    Q_INVOKABLE void disableSarCalibration()
    {
        run(QStringLiteral("/usr/bin/pkexec"),
            {QStringLiteral("/usr/libexec/nabu-sar-calibration"), QStringLiteral("disable")},
            QStringLiteral("calibration-action"), true);
    }

Q_SIGNALS:
    void stateChanged();

private Q_SLOTS:
    void onSensorPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &)
    {
        if (!m_sensorLive)
            return;
        if (interface == QLatin1String(kSarInterface))
            applySarProperties(changed);
        else if (interface == QLatin1String(kSensorsInterface))
            applySensorProperties(changed);
        Q_EMIT stateChanged();
    }

private:
    static QMap<QString, QString> keyValues(const QString &text)
    {
        QMap<QString, QString> result;
        const auto lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const qsizetype equals = line.indexOf(QLatin1Char('='));
            if (equals > 0)
                result.insert(line.left(equals).trimmed(), line.mid(equals + 1).trimmed());
        }
        return result;
    }

    static QString selectedValue(const QString &text, const QString &key)
    {
        const QRegularExpression expression(QStringLiteral("(?:^|\\n)%1=[^\\n]*\\[([^\\]]+)\\]")
                                                .arg(QRegularExpression::escape(key)));
        const auto match = expression.match(text);
        return match.hasMatch() ? match.captured(1) : QStringLiteral("unknown");
    }

    void privilegedWake(const QString &feature, bool enabled)
    {
        Q_EMIT stateChanged();
        run(QStringLiteral("/usr/bin/pkexec"),
            {QStringLiteral("/usr/libexec/nabu-wake-control"), QStringLiteral("set"), feature,
             enabled ? QStringLiteral("on") : QStringLiteral("off")}, QStringLiteral("action"), true);
    }

    void run(const QString &program, const QStringList &arguments, const QString &kind, bool refreshAfter = false)
    {
        auto *process = new QProcess(this);
        process->setProgram(program);
        process->setArguments(arguments);
        ++m_pending;
        Q_EMIT stateChanged();
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
            if (process->property("finishedHandled").toBool())
                return;
            process->setProperty("finishedHandled", true);
            m_errorText = i18n("Could not start %1", process->program());
            --m_pending;
            Q_EMIT stateChanged();
            process->deleteLater();
        });
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process, kind, refreshAfter](int exitCode, QProcess::ExitStatus status) {
                if (process->property("finishedHandled").toBool())
                    return;
                process->setProperty("finishedHandled", true);
                const QString output = QString::fromUtf8(process->readAllStandardOutput());
                const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
                if (status != QProcess::NormalExit || exitCode != 0)
                    m_errorText = error.isEmpty() ? i18n("The requested operation failed.") : error;
                else
                    applyResult(kind, output);
                --m_pending;
                Q_EMIT stateChanged();
                process->deleteLater();
                if (refreshAfter)
                    QTimer::singleShot(250, this, &NabuSettings::refresh);
            });
        process->start();
    }

    void applyResult(const QString &kind, const QString &output)
    {
        const auto values = keyValues(output);
        if (kind == QLatin1String("flash-status")) {
            m_flashlightKnown = true;
            m_flashlightAvailable = true;
            const QStringList fields = output.trimmed().split(QRegularExpression(QStringLiteral("\\s+")));
            m_flashlightEnabled = fields.value(0) == QLatin1String("on");
            bool ok = false;
            const int brightness = fields.value(1).toInt(&ok);
            if (ok && brightness >= 1 && brightness <= 100)
                m_flashlightBrightness = brightness;
        } else if (kind == QLatin1String("wake-status")) {
            m_doubleTapKnown = true;
            m_doubleTapAvailable = values.value(QStringLiteral("double_tap_available")) == QLatin1String("1");
            m_doubleTapEnabled = values.value(QStringLiteral("double_tap_enabled")) == QLatin1String("1");
            m_tiltWakeKnown = true;
            m_tiltWakeAvailable = values.value(QStringLiteral("tilt_wake_available")) == QLatin1String("1");
            m_tiltWakeEnabled = values.value(QStringLiteral("tilt_wake_enabled")) == QLatin1String("1");
            m_tiltWakeReports = values.value(QStringLiteral("tilt_wake_reports")).toULongLong();
        } else if (kind == QLatin1String("display-status")) {
            const qsizetype start = output.indexOf(QRegularExpression(QStringLiteral("(?:^|\\n)Output:\\s+\\d+\\s+DSI-1\\b")));
            const QString panel = start >= 0 ? output.mid(start) : QString();
            const auto rotate = QRegularExpression(QStringLiteral("Auto Rotate Policy:\\s*(\\S+)")).match(panel);
            const auto brightness = QRegularExpression(QStringLiteral("Automatic brightness:\\s*([^\\n]+)")).match(panel);
            m_autoRotateAvailable = rotate.hasMatch() && rotate.captured(1) != QLatin1String("unsupported");
            m_autoRotateEnabled = rotate.hasMatch() && rotate.captured(1) == QLatin1String("always");
            m_autoBrightnessAvailable = brightness.hasMatch() && !brightness.captured(1).startsWith(QLatin1String("unsupported"));
            m_autoBrightnessEnabled = brightness.hasMatch() && brightness.captured(1).contains(QLatin1String("enabled"));
        } else if (kind == QLatin1String("usb-status")) {
            m_usbMode = selectedValue(output, QStringLiteral("data"));
            m_usbPowerRole = selectedValue(output, QStringLiteral("power"));
        } else if (kind == QLatin1String("usb-gadget-status")) {
            m_usbGadgetState = values.value(QStringLiteral("gadget"), QStringLiteral("inactive"));
            QStringList services;
            for (const auto &[key, label] : std::initializer_list<std::pair<QString, QString>>{
                     {QStringLiteral("mtp"), i18n("MTP")}, {QStringLiteral("adb"), i18n("ADB")},
                     {QStringLiteral("network"), i18n("Network")}, {QStringLiteral("serial"), i18n("Serial")},
                     {QStringLiteral("ssh"), i18n("SSH")}}) {
                if (values.value(key) == QLatin1String("ready"))
                    services.append(label);
            }
            m_usbServiceSummary = services.isEmpty() ? i18n("No sharing service is ready") : services.join(QStringLiteral(" · "));
        } else if (kind == QLatin1String("accessory-status")) {
            m_accessoryKnown = true;
            m_penPaired = values.value(QStringLiteral("pen_paired")) == QLatin1String("1");
            m_penConnected = values.value(QStringLiteral("pen_connected")) == QLatin1String("1");
            m_penAddress = values.value(QStringLiteral("pen_address"));
            m_penName = values.value(QStringLiteral("pen_name"));
            m_penBattery = values.value(QStringLiteral("pen_battery"), QStringLiteral("-1")).toInt();
            m_penCharging = values.value(QStringLiteral("pen_charging")) == QLatin1String("1");
            m_penChargeLimit = values.value(QStringLiteral("pen_charge_limit"), QStringLiteral("-1")).toInt();
            m_keyboardAttached = values.value(QStringLiteral("keyboard_attached")) == QLatin1String("1");
        } else if (kind == QLatin1String("calibration-action")) {
            clearCalibration();
        }
    }

    QVariantMap getAll(const char *object, const char *interface)
    {
        QDBusInterface properties(QString::fromLatin1(kSensorService), QString::fromLatin1(object),
            QStringLiteral("org.freedesktop.DBus.Properties"), QDBusConnection::systemBus());
        const QDBusReply<QVariantMap> reply = properties.call(QStringLiteral("GetAll"), QString::fromLatin1(interface));
        return reply.isValid() ? reply.value() : QVariantMap{};
    }

    void refreshSensorSnapshot()
    {
        const QVariantMap sar = getAll(kSarObject, kSarInterface);
        const QVariantMap sensors = getAll(kSensorsObject, kSensorsInterface);
        if (sar.isEmpty()) {
            m_gripAvailable = false;
            m_sensorSummary = i18n("Sensor service is unavailable");
        } else {
            applySarProperties(sar);
        }
        if (!sensors.isEmpty())
            applySensorProperties(sensors);
        Q_EMIT stateChanged();
    }

    void applySarProperties(const QVariantMap &properties)
    {
        m_gripAvailable = properties.value(QStringLiteral("Available")).toBool();
        m_gripMappingEnabled = properties.value(QStringLiteral("MappingEnabled")).toBool();
        m_gripHoldAwakeEnabled = properties.value(QStringLiteral("HoldAwakeEnabled")).toBool();
        m_gripState = properties.value(QStringLiteral("GripState"), QStringLiteral("unknown")).toString();
        m_gripDeltas = doubleList(properties.value(QStringLiteral("Deltas")));
        m_gripChannels = numberList(m_gripDeltas);
        m_gripRawValues = numberList(doubleList(properties.value(QStringLiteral("RawValues"))));
        m_gripBaselines = numberList(doubleList(properties.value(QStringLiteral("Baselines"))));
        m_gripSampleQuality = properties.value(QStringLiteral("SampleQuality"), QStringLiteral("unknown")).toString();
        m_gripDataUsable = properties.value(QStringLiteral("DataUsable")).toBool();
        m_gripDataChanging = properties.value(QStringLiteral("DataChanging")).toBool();
        m_gripIdenticalSamples = properties.value(QStringLiteral("ConsecutiveIdenticalSamples")).toInt();
        m_gripSaturatedChannelMask = properties.value(QStringLiteral("SaturatedChannelMask")).toInt();
        const qulonglong previousSequence = m_gripSampleSequence;
        m_gripSampleSequence = properties.value(QStringLiteral("SampleSequence")).toULongLong();
        if (m_calibrationPhase != QLatin1String("idle") && !m_gripDataUsable) {
            m_calibrationTimer.stop();
            m_calibrationPhase = QStringLiteral("idle");
            m_calibrationReady = false;
            m_calibrationMessage = i18n("Capture stopped because ADUX1050 data became constant, saturated, or stale.");
        }
        if (m_calibrationPhase != QLatin1String("idle") && m_gripSampleSequence != previousSequence)
            captureCalibrationSample();
    }

    void applySensorProperties(const QVariantMap &properties)
    {
        if (!properties.contains(QStringLiteral("Algorithms")))
            return;
        const QDBusArgument argument = properties.value(QStringLiteral("Algorithms")).value<QDBusArgument>();
        if (argument.currentType() != QDBusArgument::ArrayType)
            return;
        QStringList available;
        QStringList monitoring;
        QStringList reports;
        QStringList fresh;
        QStringList discoveredOnly;
        int total = 0;
        argument.beginArray();
        while (!argument.atEnd()) {
            QVariantMap entry;
            argument >> entry;
            ++total;
            const QString name = entry.value(QStringLiteral("DataType"), QStringLiteral("unknown")).toString();
            const bool isAvailable = entry.value(QStringLiteral("Available")).toBool();
            const bool isMonitoring = entry.value(QStringLiteral("Monitoring")).toBool();
            const bool hasReport = entry.value(QStringLiteral("ReportObserved")).toBool();
            if (isAvailable)
                available.append(name);
            if (isMonitoring)
                monitoring.append(name);
            if (hasReport)
                reports.append(name);
            if (entry.value(QStringLiteral("ReportFresh")).toBool())
                fresh.append(name);
            if (isAvailable && !isMonitoring)
                discoveredOnly.append(name);
        }
        argument.endArray();
        m_sensorSummary = i18n("%1 of %2 firmware endpoints available; %3 monitored; %4 reporting",
            available.size(), total, monitoring.size(), reports.size());
        m_sensorAvailable = available.join(QStringLiteral(", "));
        m_sensorMonitoring = monitoring.join(QStringLiteral(", "));
        m_sensorReports = reports.join(QStringLiteral(", "));
        m_sensorFresh = fresh.join(QStringLiteral(", "));
        m_sensorDiscoveredOnly = discoveredOnly.join(QStringLiteral(", "));
    }

    void captureCalibrationSample()
    {
        if (m_calibrationPhase == QLatin1String("idle") || !m_gripAvailable
            || !m_gripDataUsable
            || m_gripSampleSequence == 0 || m_gripSampleSequence == m_lastCalibrationSequence
            || m_gripDeltas.size() < 3)
            return;
        double peak = 0.0;
        for (int channel = 0; channel < 3; ++channel) {
            if (m_calibrationChannelMask & (1 << channel))
                peak = std::max(peak, std::abs(m_gripDeltas.at(channel)));
        }
        if (!std::isfinite(peak))
            return;
        m_lastCalibrationSequence = m_gripSampleSequence;
        QList<double> &samples = m_calibrationPhase == QLatin1String("released")
            ? m_calibrationReleased : m_calibrationHeld;
        samples.append(peak);
        if (samples.size() >= kCalibrationSamples) {
            m_calibrationTimer.stop();
            m_calibrationPhase = QStringLiteral("idle");
            updateCalibrationProposal();
        }
    }

    static double percentile(QList<double> values, double fraction)
    {
        std::sort(values.begin(), values.end());
        const int last = static_cast<int>(values.size() - 1);
        const int index = std::clamp(static_cast<int>(std::lround(fraction * last)), 0, last);
        return values.at(index);
    }

    void updateCalibrationProposal()
    {
        m_calibrationReady = false;
        if (m_calibrationReleased.size() < kCalibrationSamples || m_calibrationHeld.size() < kCalibrationSamples) {
            m_calibrationMessage = i18n("Collect at least %1 released and %1 held samples.", kCalibrationSamples);
            return;
        }
        const double releasedHigh = percentile(m_calibrationReleased, 0.9);
        const double heldLow = percentile(m_calibrationHeld, 0.1);
        const double gap = heldLow - releasedHigh;
        const double minimumGap = std::max(25.0, std::abs(releasedHigh) * 0.1);
        if (!std::isfinite(gap) || gap < minimumGap) {
            m_calibrationMessage = i18n("The released and held ranges overlap. Do not apply this capture; adjust channels or repeat it.");
            return;
        }
        m_proposedReleasedThreshold = releasedHigh + gap * 0.35;
        m_proposedHeldThreshold = releasedHigh + gap * 0.65;
        m_calibrationReady = true;
        m_calibrationMessage = i18n("Ranges are separated. Review the proposed thresholds, then apply explicitly.");
    }

    void refreshCover()
    {
        QDBusInterface manager(QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("/org/kde/Solid/PowerManagement"),
            QStringLiteral("org.kde.Solid.PowerManagement"), QDBusConnection::sessionBus());
        const QDBusReply<bool> present = manager.call(QStringLiteral("isLidPresent"));
        const QDBusReply<bool> closed = manager.call(QStringLiteral("isLidClosed"));
        QDBusInterface action(QStringLiteral("org.kde.Solid.PowerManagement"),
            QStringLiteral("/org/kde/Solid/PowerManagement/Actions/HandleButtonEvents"),
            QStringLiteral("org.kde.Solid.PowerManagement.Actions.HandleButtonEvents"), QDBusConnection::sessionBus());
        const QDBusReply<bool> enabled = action.call(QStringLiteral("triggersLidAction"));
        m_coverAvailable = present.isValid() && present.value();
        m_coverClosed = closed.isValid() && closed.value();
        m_coverSleepEnabled = enabled.isValid() && enabled.value();
        Q_EMIT stateChanged();
    }

    void refreshReports()
    {
        QFile camera(QStringLiteral("/run/nabu-camera/calibration.json"));
        if (camera.open(QIODevice::ReadOnly)) {
            const int count = QJsonDocument::fromJson(camera.readAll()).object()
                                  .value(QStringLiteral("sensors")).toArray().size();
            m_cameraSummary = i18np("%1 calibrated camera module", "%1 calibrated camera modules", count);
        } else {
            m_cameraSummary = i18n("Camera calibration report is unavailable");
        }
        QFile provenance(QStringLiteral("/run/nabu-hardware/provenance.json"));
        if (provenance.open(QIODevice::ReadOnly)) {
            const int count = QJsonDocument::fromJson(provenance.readAll()).object()
                                  .value(QStringLiteral("dtbo")).toObject()
                                  .value(QStringLiteral("entries")).toArray().size();
            m_provenanceSummary = i18np("Read-only hardware provenance loaded; %1 DTBO overlay",
                "Read-only hardware provenance loaded; %1 DTBO overlays", count);
        } else {
            m_provenanceSummary = i18n("Hardware provenance report is unavailable");
        }
        Q_EMIT stateChanged();
    }

    int m_pending = 0;
    QString m_errorText;
    bool m_flashlightKnown = false;
    bool m_flashlightAvailable = false;
    bool m_flashlightEnabled = false;
    int m_flashlightBrightness = 13;
    bool m_doubleTapKnown = false;
    bool m_doubleTapAvailable = false;
    bool m_doubleTapEnabled = false;
    bool m_tiltWakeKnown = false;
    bool m_tiltWakeAvailable = false;
    bool m_tiltWakeEnabled = false;
    qulonglong m_tiltWakeReports = 0;
    bool m_autoRotateAvailable = false;
    bool m_autoRotateEnabled = false;
    bool m_autoBrightnessAvailable = false;
    bool m_autoBrightnessEnabled = false;
    bool m_coverAvailable = false;
    bool m_coverClosed = false;
    bool m_coverSleepEnabled = false;
    bool m_gripAvailable = false;
    bool m_gripMappingEnabled = false;
    bool m_gripHoldAwakeEnabled = false;
    QString m_gripState = QStringLiteral("unknown");
    QString m_gripChannels;
    QString m_gripRawValues;
    QString m_gripBaselines;
    QString m_gripSampleQuality = QStringLiteral("unknown");
    bool m_gripDataUsable = false;
    bool m_gripDataChanging = false;
    int m_gripIdenticalSamples = 0;
    int m_gripSaturatedChannelMask = 0;
    QList<double> m_gripDeltas;
    qulonglong m_gripSampleSequence = 0;
    bool m_sensorLive = false;
    QString m_sensorSummary;
    QString m_sensorAvailable;
    QString m_sensorMonitoring;
    QString m_sensorReports;
    QString m_sensorFresh;
    QString m_sensorDiscoveredOnly;
    QString m_calibrationPhase = QStringLiteral("idle");
    int m_calibrationChannelMask = 5;
    QList<double> m_calibrationReleased;
    QList<double> m_calibrationHeld;
    bool m_calibrationReady = false;
    double m_proposedHeldThreshold = 0.0;
    double m_proposedReleasedThreshold = 0.0;
    QString m_calibrationMessage;
    qulonglong m_lastCalibrationSequence = 0;
    QTimer m_calibrationTimer;
    bool m_accessoryKnown = false;
    bool m_penPaired = false;
    bool m_penConnected = false;
    QString m_penAddress;
    QString m_penName;
    int m_penBattery = -1;
    bool m_penCharging = false;
    int m_penChargeLimit = -1;
    bool m_keyboardAttached = false;
    QString m_usbMode = QStringLiteral("unknown");
    QString m_usbPowerRole = QStringLiteral("unknown");
    QString m_usbGadgetState = QStringLiteral("inactive");
    QString m_usbServiceSummary;
    QString m_cameraSummary;
    QString m_provenanceSummary;
};

K_PLUGIN_CLASS_WITH_JSON(NabuSettings, "kcm_nabu.json")

#include "kcm_nabu.moc"
