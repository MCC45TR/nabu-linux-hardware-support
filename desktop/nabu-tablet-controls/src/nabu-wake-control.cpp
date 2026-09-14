// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 mcc45tr <mcc45tr@gmail.com>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>

#include <unistd.h>

namespace {
constexpr auto kDoubleTapPath = "/sys/bus/spi/devices/spi0.0/double_tap_to_wake";
constexpr auto kDoubleTapStatePath = "/var/lib/nabu-wake/double-tap-enabled";
constexpr auto kModelPath = "/sys/firmware/devicetree/base/model";

bool parseSwitch(const QString &value, bool *enabled)
{
    if (value == QLatin1String("on") || value == QLatin1String("1") || value == QLatin1String("true")) {
        *enabled = true;
        return true;
    }
    if (value == QLatin1String("off") || value == QLatin1String("0") || value == QLatin1String("false")) {
        *enabled = false;
        return true;
    }
    return false;
}

bool isNabu()
{
    QFile model(QString::fromLatin1(kModelPath));
    return model.open(QIODevice::ReadOnly)
        && QString::fromUtf8(model.readAll()).contains(QStringLiteral("Xiaomi Pad 5"), Qt::CaseInsensitive);
}

bool readBooleanFile(const QString &path, bool *enabled)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray value = file.readAll().trimmed();
    if (value == "1" || value == "enabled") {
        *enabled = true;
        return true;
    }
    if (value == "0" || value == "disabled") {
        *enabled = false;
        return true;
    }
    return false;
}

bool writeBooleanFile(const QString &path, bool enabled, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = file.errorString();
        return false;
    }
    const QByteArray value = enabled ? QByteArrayLiteral("1\n") : QByteArrayLiteral("0\n");
    if (file.write(value) != value.size() || !file.flush()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

bool persistDoubleTap(bool enabled, QString *error)
{
    QSaveFile file(QString::fromLatin1(kDoubleTapStatePath));
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

QVariant wakeProperty(const QString &name)
{
    QDBusInterface properties(QStringLiteral("org.senemos.Nabu.Wake"),
        QStringLiteral("/org/senemos/Nabu/Wake"), QStringLiteral("org.freedesktop.DBus.Properties"),
        QDBusConnection::systemBus());
    QDBusReply<QVariant> reply = properties.call(QStringLiteral("Get"),
        QStringLiteral("org.senemos.Nabu.Wake1"), name);
    return reply.isValid() ? reply.value() : QVariant();
}

int status()
{
    QTextStream out(stdout);
    bool doubleTap = false;
    const bool nabu = isNabu();
    const bool doubleTapAvailable = nabu && QFile::exists(QString::fromLatin1(kDoubleTapPath));
    if (doubleTapAvailable)
        readBooleanFile(QString::fromLatin1(kDoubleTapPath), &doubleTap);
    const QVariant tiltAvailable = wakeProperty(QStringLiteral("TiltWakeAvailable"));
    const QVariant tiltEnabled = wakeProperty(QStringLiteral("TiltWakeEnabled"));
    const QVariant tiltReports = wakeProperty(QStringLiteral("TiltWakeReports"));

    out << "device_nabu=" << (nabu ? 1 : 0) << '\n'
        << "double_tap_available=" << (doubleTapAvailable ? 1 : 0) << '\n'
        << "double_tap_enabled=" << (doubleTap ? 1 : 0) << '\n'
        << "tilt_wake_service=" << (tiltAvailable.isValid() ? 1 : 0) << '\n'
        << "tilt_wake_available=" << (tiltAvailable.toBool() ? 1 : 0) << '\n'
        << "tilt_wake_enabled=" << (tiltEnabled.toBool() ? 1 : 0) << '\n'
        << "tilt_wake_reports=" << tiltReports.toULongLong() << '\n';
    return 0;
}

int setDoubleTap(bool enabled)
{
    if (geteuid() != 0) {
        qCritical("root authorization is required");
        return 77;
    }
    if (!isNabu() || !QFile::exists(QString::fromLatin1(kDoubleTapPath))) {
        qCritical("Nabu double-tap runtime control is unavailable");
        return 69;
    }
    bool previous = false;
    if (!readBooleanFile(QString::fromLatin1(kDoubleTapPath), &previous)) {
        qCritical("cannot read current double-tap runtime control");
        return 74;
    }
    QString error;
    if (!writeBooleanFile(QString::fromLatin1(kDoubleTapPath), enabled, &error)) {
        qCritical("cannot update double-tap runtime control: %s", qPrintable(error));
        return 74;
    }
    if (!persistDoubleTap(enabled, &error)) {
        QString ignored;
        writeBooleanFile(QString::fromLatin1(kDoubleTapPath), previous, &ignored);
        qCritical("cannot persist double-tap setting: %s", qPrintable(error));
        return 74;
    }
    return 0;
}

int setTiltWake(bool enabled)
{
    if (geteuid() != 0) {
        qCritical("root authorization is required");
        return 77;
    }
    QDBusInterface wake(QStringLiteral("org.senemos.Nabu.Wake"),
        QStringLiteral("/org/senemos/Nabu/Wake"), QStringLiteral("org.senemos.Nabu.Wake1"),
        QDBusConnection::systemBus());
    QDBusReply<void> reply = wake.call(QStringLiteral("SetTiltWakeEnabled"), enabled);
    if (!reply.isValid()) {
        qCritical("cannot update tilt-to-wake: %s", qPrintable(reply.error().message()));
        return 69;
    }
    return 0;
}

int applyPersisted()
{
    if (geteuid() != 0)
        return 77;
    if (!isNabu())
        return 0;
    bool enabled = false;
    if (!readBooleanFile(QString::fromLatin1(kDoubleTapStatePath), &enabled))
        return 0;
    if (!QFile::exists(QString::fromLatin1(kDoubleTapPath)))
        return 0;
    QString error;
    if (!writeBooleanFile(QString::fromLatin1(kDoubleTapPath), enabled, &error)) {
        qCritical("cannot restore double-tap setting: %s", qPrintable(error));
        return 74;
    }
    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.size() == 2 && arguments.at(1) == QLatin1String("status"))
        return status();
    if (arguments.size() == 2 && arguments.at(1) == QLatin1String("apply"))
        return applyPersisted();
    if (arguments.size() == 4 && arguments.at(1) == QLatin1String("set")) {
        bool enabled = false;
        if (!parseSwitch(arguments.at(3), &enabled)) {
            qCritical("value must be on or off");
            return 64;
        }
        if (arguments.at(2) == QLatin1String("double-tap"))
            return setDoubleTap(enabled);
        if (arguments.at(2) == QLatin1String("tilt-wake"))
            return setTiltWake(enabled);
    }
    qCritical("usage: nabu-wake-control status | apply | set {double-tap|tilt-wake} {on|off}");
    return 64;
}
