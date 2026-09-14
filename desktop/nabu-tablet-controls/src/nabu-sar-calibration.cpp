// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 mcc45tr <mcc45tr@gmail.com>

#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>

#include <unistd.h>

namespace {
constexpr auto kConfigPath = "/etc/nabu-sar.conf";
constexpr auto kModelPath = "/sys/firmware/devicetree/base/model";

bool isNabu()
{
    QFile model(QString::fromLatin1(kModelPath));
    return model.open(QIODevice::ReadOnly)
        && QString::fromUtf8(model.readAll()).contains(QStringLiteral("Xiaomi Pad 5"), Qt::CaseInsensitive);
}

bool writeConfig(const QByteArray &contents, QString *error)
{
    QSaveFile output(QString::fromLatin1(kConfigPath));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        *error = output.errorString();
        return false;
    }
    output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
        | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    if (output.write(contents) != contents.size() || !output.commit()) {
        *error = output.errorString();
        return false;
    }
    return true;
}

bool restartService(QString *error)
{
    QDBusInterface systemd(QStringLiteral("org.freedesktop.systemd1"),
        QStringLiteral("/org/freedesktop/systemd1"), QStringLiteral("org.freedesktop.systemd1.Manager"),
        QDBusConnection::systemBus());
    const QDBusReply<QDBusObjectPath> reply = systemd.call(QStringLiteral("RestartUnit"),
        QStringLiteral("nabu-sar-service.service"), QStringLiteral("replace"));
    if (!reply.isValid()) {
        *error = reply.error().message();
        return false;
    }
    return true;
}

int installConfig(const QByteArray &contents)
{
    if (geteuid() != 0) {
        qCritical("root authorization is required");
        return 77;
    }
    if (!isNabu()) {
        qCritical("this operation is restricted to Xiaomi Pad 5 (nabu)");
        return 69;
    }
    QFile oldFile(QString::fromLatin1(kConfigPath));
    QByteArray previous;
    const bool hadPrevious = oldFile.open(QIODevice::ReadOnly);
    if (hadPrevious)
        previous = oldFile.readAll();

    QString error;
    if (!writeConfig(contents, &error)) {
        qCritical("cannot atomically write SAR configuration: %s", qPrintable(error));
        return 74;
    }
    if (restartService(&error))
        return 0;

    if (hadPrevious) {
        QString rollbackError;
        if (!writeConfig(previous, &rollbackError))
            qCritical("service restart and rollback failed: %s; %s", qPrintable(error), qPrintable(rollbackError));
        else
            qCritical("service restart failed; previous configuration restored: %s", qPrintable(error));
    } else {
        qCritical("service restart failed after first configuration: %s", qPrintable(error));
    }
    return 74;
}

QByteArray config(bool enabled, int mask, double held, double released, int debounce)
{
    return QByteArrayLiteral("# Managed by plasma-nabu-kcm; firmware and calibration storage are never modified.\n"
                             "[Mapping]\nEnabled=")
        + (enabled ? "true\n" : "false\n")
        + "ChannelMask=" + QByteArray::number(mask) + '\n'
        + "HeldThreshold=" + QByteArray::number(held, 'f', 3) + '\n'
        + "ReleasedThreshold=" + QByteArray::number(released, 'f', 3) + '\n'
        + "DebounceSamples=" + QByteArray::number(debounce) + '\n';
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (arguments.size() == 2 && arguments.at(1) == QLatin1String("disable"))
        return installConfig(config(false, 0, 0.0, 0.0, 3));

    if (arguments.size() == 6 && arguments.at(1) == QLatin1String("apply")) {
        static const QRegularExpression integer(QStringLiteral("^[0-9]{1,3}$"));
        static const QRegularExpression decimal(QStringLiteral("^[0-9]{1,8}(?:\\.[0-9]{1,3})?$"));
        if (!integer.match(arguments.at(2)).hasMatch() || !decimal.match(arguments.at(3)).hasMatch()
            || !decimal.match(arguments.at(4)).hasMatch() || !integer.match(arguments.at(5)).hasMatch()) {
            qCritical("invalid calibration number format");
            return 64;
        }
        bool maskOk = false;
        bool heldOk = false;
        bool releasedOk = false;
        bool debounceOk = false;
        const int mask = arguments.at(2).toInt(&maskOk);
        const double held = arguments.at(3).toDouble(&heldOk);
        const double released = arguments.at(4).toDouble(&releasedOk);
        const int debounce = arguments.at(5).toInt(&debounceOk);
        if (!maskOk || mask < 1 || mask > 7 || !heldOk || !releasedOk
            || released < 0.0 || held <= released || held > 1000000.0
            || !debounceOk || debounce < 1 || debounce > 100) {
            qCritical("calibration values are outside the fail-closed limits");
            return 64;
        }
        return installConfig(config(true, mask, held, released, debounce));
    }
    qCritical("usage: nabu-sar-calibration apply MASK HELD RELEASED DEBOUNCE | disable");
    return 64;
}
