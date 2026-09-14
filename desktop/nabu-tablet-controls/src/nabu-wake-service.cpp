// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 mcc45tr <mcc45tr@gmail.com>

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QFile>
#include <QDateTime>
#include <QSaveFile>
#include <QTimer>
#include <QVariantMap>

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
constexpr auto kStatePath = "/var/lib/nabu-wake/tilt-wake-enabled";
constexpr auto kSarService = "org.senemos.Nabu.Sar";
constexpr auto kSarObject = "/org/senemos/Nabu/Sensors";
constexpr auto kSarInterface = "org.senemos.Nabu.Sensors1";

bool saveEnabled(bool enabled, QString *error)
{
    QSaveFile file(QString::fromLatin1(kStatePath));
    if (!file.open(QIODevice::WriteOnly)) {
        *error = file.errorString();
        return false;
    }
    const QByteArray value = enabled ? QByteArrayLiteral("1\n") : QByteArrayLiteral("0\n");
    if (file.write(value) != value.size() || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

bool loadEnabled()
{
    QFile file(QString::fromLatin1(kStatePath));
    return file.open(QIODevice::ReadOnly) && file.readAll().trimmed() == "1";
}
}

class WakeService;

class WakeAdaptor final : public QDBusAbstractAdaptor, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.senemos.Nabu.Wake1")
    Q_PROPERTY(bool TiltWakeAvailable READ tiltWakeAvailable NOTIFY PropertiesChanged)
    Q_PROPERTY(bool TiltWakeEnabled READ tiltWakeEnabled NOTIFY PropertiesChanged)
    Q_PROPERTY(qulonglong TiltWakeReports READ tiltWakeReports NOTIFY PropertiesChanged)

public:
    explicit WakeAdaptor(WakeService *service);
    bool tiltWakeAvailable() const;
    bool tiltWakeEnabled() const;
    qulonglong tiltWakeReports() const;

public Q_SLOTS:
    void SetTiltWakeEnabled(bool enabled);

Q_SIGNALS:
    void PropertiesChanged();

private:
    WakeService *m_service;
};

class WakeService final : public QObject
{
    Q_OBJECT
public:
    explicit WakeService(QObject *parent = nullptr)
        : QObject(parent)
        , m_enabled(loadEnabled())
        , m_watcher(QString::fromLatin1(kSarService), QDBusConnection::systemBus(),
              QDBusServiceWatcher::WatchForOwnerChange, this)
    {
        m_adaptor = new WakeAdaptor(this);
        connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &newOwner) {
                if (!newOwner.isEmpty())
                    refreshAlgorithms();
                else
                    setAvailable(false);
            });
        QDBusConnection::systemBus().connect(QString::fromLatin1(kSarService),
            QString::fromLatin1(kSarObject), QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"), this,
            SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
        QTimer::singleShot(0, this, &WakeService::refreshAlgorithms);
    }

    ~WakeService() override
    {
        if (m_uinputFd >= 0) {
            ioctl(m_uinputFd, UI_DEV_DESTROY);
            close(m_uinputFd);
        }
    }

    bool available() const { return m_available; }
    bool enabled() const { return m_enabled; }
    qulonglong reports() const { return m_reports; }

    bool setEnabled(bool enabled, QString *error)
    {
        if (enabled && !m_available) {
            *error = QStringLiteral("Sensor DSP tilt-to-wake endpoint is unavailable");
            return false;
        }
        if (enabled && !ensureUinput(error))
            return false;
        if (!saveEnabled(enabled, error))
            return false;
        if (m_enabled == enabled)
            return true;
        m_enabled = enabled;
        Q_EMIT m_adaptor->PropertiesChanged();
        return true;
    }

private Q_SLOTS:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &)
    {
        if (interface != QLatin1String(kSarInterface) || !changed.contains(QStringLiteral("Algorithms")))
            return;
        parseAlgorithms(changed.value(QStringLiteral("Algorithms")));
    }

private:
    friend class WakeAdaptor;

    void refreshAlgorithms()
    {
        QDBusInterface properties(QString::fromLatin1(kSarService), QString::fromLatin1(kSarObject),
            QStringLiteral("org.freedesktop.DBus.Properties"), QDBusConnection::systemBus());
        QDBusReply<QVariant> reply = properties.call(QStringLiteral("Get"),
            QString::fromLatin1(kSarInterface), QStringLiteral("Algorithms"));
        if (!reply.isValid()) {
            setAvailable(false);
            return;
        }
        parseAlgorithms(reply.value());
    }

    void parseAlgorithms(const QVariant &value)
    {
        const QDBusArgument argument = value.value<QDBusArgument>();
        if (argument.currentType() != QDBusArgument::ArrayType) {
            setAvailable(false);
            return;
        }
        bool found = false;
        qulonglong reportCount = 0;
        argument.beginArray();
        while (!argument.atEnd()) {
            QVariantMap entry;
            argument >> entry;
            if (entry.value(QStringLiteral("DataType")).toString() != QLatin1String("tilt_to_wake"))
                continue;
            found = entry.value(QStringLiteral("Available")).toBool();
            reportCount = entry.value(QStringLiteral("ReportCount")).toULongLong();
        }
        argument.endArray();

        const qulonglong previous = m_reports;
        const bool initialized = m_initialized;
        m_initialized = true;
        m_reports = reportCount;
        /* The firmware endpoint alone is not sufficient: only advertise the
         * bridge when the kernel can expose the restricted wake input node. */
        setAvailable(found && QFile::exists(QStringLiteral("/dev/uinput")));
        if (initialized && m_enabled && reportCount > previous)
            emitWakeKey();
        Q_EMIT m_adaptor->PropertiesChanged();
    }

    void setAvailable(bool available)
    {
        if (m_available == available)
            return;
        m_available = available;
        Q_EMIT m_adaptor->PropertiesChanged();
    }

    bool ensureUinput(QString *error)
    {
        if (m_uinputFd >= 0)
            return true;
        const int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            *error = QStringLiteral("cannot open /dev/uinput");
            return false;
        }
        uinput_setup setup{};
        qstrncpy(setup.name, "SENEMOS Nabu Wake", UINPUT_MAX_NAME_SIZE);
        setup.id.bustype = BUS_VIRTUAL;
        setup.id.vendor = 0x2717;
        setup.id.product = 0x0005;
        if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0
            || ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0
            || ioctl(fd, UI_SET_KEYBIT, KEY_WAKEUP) < 0
            || ioctl(fd, UI_DEV_SETUP, &setup) < 0
            || ioctl(fd, UI_DEV_CREATE) < 0) {
            close(fd);
            *error = QStringLiteral("cannot create the restricted wake input device");
            return false;
        }
        m_uinputFd = fd;
        return true;
    }

    void emitWakeKey()
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastWakeMsec < 2000)
            return;
        QString error;
        if (!ensureUinput(&error)) {
            qWarning("%s", qPrintable(error));
            return;
        }
        const input_event events[] = {
            {.time = {}, .type = EV_KEY, .code = KEY_WAKEUP, .value = 1},
            {.time = {}, .type = EV_SYN, .code = SYN_REPORT, .value = 0},
            {.time = {}, .type = EV_KEY, .code = KEY_WAKEUP, .value = 0},
            {.time = {}, .type = EV_SYN, .code = SYN_REPORT, .value = 0},
        };
        if (write(m_uinputFd, events, sizeof(events)) != static_cast<ssize_t>(sizeof(events)))
            qWarning("failed to emit complete KEY_WAKEUP sequence");
        else
            m_lastWakeMsec = now;
    }

    WakeAdaptor *m_adaptor = nullptr;
    bool m_available = false;
    bool m_enabled = false;
    bool m_initialized = false;
    qulonglong m_reports = 0;
    qint64 m_lastWakeMsec = 0;
    int m_uinputFd = -1;
    QDBusServiceWatcher m_watcher;
};

WakeAdaptor::WakeAdaptor(WakeService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

bool WakeAdaptor::tiltWakeAvailable() const { return m_service->available(); }
bool WakeAdaptor::tiltWakeEnabled() const { return m_service->enabled(); }
qulonglong WakeAdaptor::tiltWakeReports() const { return m_service->reports(); }

void WakeAdaptor::SetTiltWakeEnabled(bool enabled)
{
    QString error;
    if (!m_service->setEnabled(enabled, &error))
        sendErrorReply(QStringLiteral("org.senemos.Nabu.Wake.Error"), error);
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    WakeService service;
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.registerObject(QStringLiteral("/org/senemos/Nabu/Wake"), &service,
            QDBusConnection::ExportAdaptors)) {
        qCritical("cannot register wake service object: %s", qPrintable(bus.lastError().message()));
        return 1;
    }
    if (!bus.registerService(QStringLiteral("org.senemos.Nabu.Wake"))) {
        qCritical("cannot own wake service name: %s", qPrintable(bus.lastError().message()));
        return 1;
    }
    return application.exec();
}

#include "nabu-wake-service.moc"
