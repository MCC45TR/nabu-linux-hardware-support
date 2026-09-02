#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QTextStream>
#include <QVariantMap>

using InterfaceMap = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, InterfaceMap>;

Q_DECLARE_METATYPE(InterfaceMap)
Q_DECLARE_METATYPE(ManagedObjects)

struct PenState {
    bool paired = false;
    bool connected = false;
    QString address;
    QString name;
    QString path;
    int bluetoothBattery = -1;
    int score = -1;
};

static QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readLine()).trimmed();
}

static int readPercent(const QString &path)
{
    bool ok = false;
    const int value = readText(path).toInt(&ok);
    return ok && value >= 0 && value <= 100 ? value : -1;
}

static bool validAddress(const QString &address)
{
    static const QRegularExpression expression(
        QStringLiteral("^(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$"));
    return address != QStringLiteral("00:00:00:00:00:00")
        && expression.match(address).hasMatch();
}

static QString kernelPenAddress()
{
    const QDir devices(QStringLiteral("/sys/bus/i2c/devices"));
    for (const QString &entry : devices.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString address = readText(devices.filePath(entry + QStringLiteral("/pen_mac"))).toUpper();
        if (validAddress(address))
            return address;
    }
    return {};
}

static bool keyboardAttached()
{
    const QDir driver(QStringLiteral("/sys/bus/platform/drivers/xiaomi-nabu-keyboard"));
    for (const QString &entry : driver.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        /* The shipping Nabu cover-detect state is inverted. */
        if (readText(driver.filePath(entry + QStringLiteral("/connected"))) == QStringLiteral("0"))
            return true;
    }
    return false;
}

static bool looksLikeStylus(const QString &name)
{
    return name.contains(QStringLiteral("pen"), Qt::CaseInsensitive)
        || name.contains(QStringLiteral("stylus"), Qt::CaseInsensitive)
        || name.contains(QStringLiteral("pencil"), Qt::CaseInsensitive);
}

static PenState queryPen(const QString &kernelAddress, const QString &wantedAddress)
{
    PenState selected;
    QDBusInterface manager(QStringLiteral("org.bluez"), QStringLiteral("/"),
                           QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                           QDBusConnection::systemBus());
    const QDBusReply<ManagedObjects> reply = manager.call(QStringLiteral("GetManagedObjects"));
    if (!reply.isValid())
        return selected;

    for (auto object = reply.value().cbegin(); object != reply.value().cend(); ++object) {
        const QVariantMap device = object.value().value(QStringLiteral("org.bluez.Device1"));
        if (device.isEmpty() || !device.value(QStringLiteral("Paired")).toBool())
            continue;

        const QString address = device.value(QStringLiteral("Address")).toString().toUpper();
        QString name = device.value(QStringLiteral("Alias")).toString();
        if (name.isEmpty())
            name = device.value(QStringLiteral("Name")).toString();

        int score = looksLikeStylus(name) ? 10 : -1;
        if (validAddress(kernelAddress) && address == kernelAddress)
            score = 100;
        if (validAddress(wantedAddress) && address == wantedAddress)
            score = 200;
        if (score <= selected.score)
            continue;

        selected.paired = true;
        selected.connected = device.value(QStringLiteral("Connected")).toBool();
        selected.address = address;
        selected.name = name.isEmpty() ? QStringLiteral("Xiaomi Smart Pen") : name;
        selected.path = object.key().path();
        selected.score = score;
        const QVariantMap battery = object.value().value(QStringLiteral("org.bluez.Battery1"));
        if (!battery.isEmpty())
            selected.bluetoothBattery = battery.value(QStringLiteral("Percentage"), -1).toInt();
    }
    return selected;
}

static int connectPen(const PenState &pen)
{
    if (!pen.paired || pen.path.isEmpty())
        return 1;
    QDBusInterface device(QStringLiteral("org.bluez"), pen.path,
                          QStringLiteral("org.bluez.Device1"),
                          QDBusConnection::systemBus());
    const QDBusMessage reply = device.call(QStringLiteral("Connect"));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        QTextStream(stderr) << "cannot connect stylus: " << reply.errorMessage() << '\n';
        return 1;
    }
    return 0;
}

static void printStatus(const PenState &pen)
{
    const int dockBattery = readPercent(QStringLiteral("/sys/class/power_supply/idtp9418/capacity"));
    const int online = readText(QStringLiteral("/sys/class/power_supply/idtp9418/online")) == QStringLiteral("1");
    const int chargeLimit = readPercent(
        QStringLiteral("/sys/class/power_supply/idtp9418/charge_control_end_threshold"));
    const int battery = online || pen.bluetoothBattery < 0 ? dockBattery : pen.bluetoothBattery;
    QString safeName = pen.name;
    safeName.replace(QRegularExpression(QStringLiteral("[\\r\\n]+")), QStringLiteral(" "));
    QTextStream output(stdout);
    output << "pen_paired=" << int(pen.paired) << '\n'
           << "pen_connected=" << int(pen.connected) << '\n'
           << "pen_address=" << (pen.paired ? pen.address : QStringLiteral("unknown")) << '\n'
           << "pen_name=" << (pen.paired ? safeName : QStringLiteral("unknown")) << '\n'
           << "pen_battery=" << battery << '\n'
           << "pen_charging=" << online << '\n'
           << "pen_charge_limit=" << chargeLimit << '\n'
           << "keyboard_attached=" << int(keyboardAttached()) << '\n';
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    qDBusRegisterMetaType<InterfaceMap>();
    qDBusRegisterMetaType<ManagedObjects>();

    const QStringList arguments = application.arguments();
    const bool connect = arguments.size() == 3 && arguments.at(1) == QStringLiteral("connect");
    if ((!connect && (arguments.size() != 2 || arguments.at(1) != QStringLiteral("status")))
        || (connect && !validAddress(arguments.at(2)))) {
        QTextStream(stderr) << "usage: nabu-accessory-state status|connect MAC\n";
        return 2;
    }

    const QString wantedAddress = connect ? arguments.at(2).toUpper() : QString();
    const PenState pen = queryPen(kernelPenAddress(), wantedAddress);
    if (connect)
        return connectPen(pen);
    printStatus(pen);
    return 0;
}
