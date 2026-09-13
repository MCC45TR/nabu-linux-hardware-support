// SPDX-License-Identifier: MIT
// Privacy-preserving, read-only Nabu hardware provenance inventory.
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#include <QtEndian>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace {
constexpr quint32 DtMagic = 0xD7B7AB1E, FdtMagic = 0xD00DFEED;
constexpr quint32 FdtBegin = 1, FdtEndNode = 2, FdtProp = 3, FdtNop = 4, FdtEnd = 9;
constexpr quint32 MaxDtboEntries = 64, MaxDtboSize = 64 * 1024 * 1024;

struct Args {
    QString sys = "/sys", dev = "/dev", firmware = "/usr/lib/firmware", etc = "/etc";
    QString cmdline = "/proc/cmdline", output = "/run/nabu-hardware/provenance.json";
    bool stdoutReport = false, strict = false, allowRegular = false;
};
struct ParsedDtbo { QJsonObject metadata; QJsonArray entries; QByteArray raw; };

const QList<QPair<QString, QString>> firmwareFiles{
    {"adsp", "qcom/sm8150/xiaomi/nabu/adsp.mbn"}, {"cdsp", "qcom/sm8150/xiaomi/nabu/cdsp.mbn"},
    {"modem", "qcom/sm8150/xiaomi/nabu/modem.mbn"}, {"slpi", "qcom/sm8150/xiaomi/nabu/slpi_nb.mbn"},
    {"wlan-mcu", "qcom/sm8150/xiaomi/nabu/wlanmdsp.mbn"},
    {"sensor-registry", "qcom/sm8150/xiaomi/nabu/hexagonfs/sensors/sns_reg.conf"},
    {"wifi-board", "ath10k/WCN3990/hw1.0/board-2.bin"}, {"wifi-firmware", "ath10k/WCN3990/hw1.0/firmware-5.bin"},
    {"bluetooth-patch", "qca/crbtfw32.tlv"}, {"bluetooth-nv-template", "qca/crnv32.bin"},
};
const QList<QPair<QString, QList<QString>>> partitionGroups{
    {"dsp", {"dsp_a", "dsp_b"}}, {"modem", {"modem_a", "modem_b"}},
    {"bluetooth", {"bluetooth_a", "bluetooth_b"}}, {"wifi-calibration-container", {"persist"}},
};
const QStringList modemNv{"modemst1", "modemst2", "fsg", "fsc"};
const QStringList panelRevisions{"36-02-0b", "42-02-0a"};
const QList<QPair<QString, QList<QByteArray>>> featureMarkers{
    {"xiaomi-nabu", {"xiaomi,nabu", "xiaomi nabu"}}, {"panel-36-02-0b", {"36_02_0b", "36-02-0b"}},
    {"panel-42-02-0a", {"42_02_0a", "42-02-0a"}}, {"cs35l41-speakers", {"cs35l41"}},
    {"k82-sunwoda-battery", {"k82_sunwoda", "sunwoda_8720"}}, {"xiaomi-keyboard", {"xiaomi-keyboard", "xiaomi_keyboard"}},
    {"ov13b10-rear-camera", {"ov13b10"}}, {"ov8856-front-camera", {"ov8856"}},
    {"nt36523-touch-display", {"nt36523"}}, {"usb-power-delivery", {"usbpd", "usb-pd", "typec"}},
};

[[noreturn]] void fail(const QString &message) { throw std::runtime_error(message.toStdString()); }
quint32 be32(const QByteArray &data, qsizetype offset) {
    if (offset < 0 || offset + 4 > data.size()) fail("bounded structure read overflow");
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + offset));
}
quint64 be64(const QByteArray &data, qsizetype offset) {
    if (offset < 0 || offset + 8 > data.size()) fail("bounded structure read overflow");
    return qFromBigEndian<quint64>(reinterpret_cast<const uchar *>(data.constData() + offset));
}
quint64 addChecked(quint64 a, quint64 b, const char *context) {
    if (b > std::numeric_limits<quint64>::max() - a) fail(context);
    return a + b;
}
qsizetype align4(qsizetype value) { return (value + 3) & ~qsizetype(3); }

QByteArray readBytes(const QString &path, qint64 maximum) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
    return file.read(maximum);
}
QString readText(const QString &path, qint64 maximum = 4096) {
    QByteArray data = readBytes(path, maximum); while (data.endsWith('\0') || data.endsWith('\n')) data.chop(1);
    return QString::fromUtf8(data);
}
QStringList readStringList(const QString &path) {
    QStringList result; for (const auto &part : readBytes(path, 4096).split('\0')) if (!part.isEmpty()) result << QString::fromUtf8(part); return result;
}
QString modeString(mode_t mode) { return QStringLiteral("%1").arg(mode & 07777, 4, 8, QLatin1Char('0')); }

QString partitionPath(const Args &args, const QString &label) {
    const QFileInfo devRoot(args.dev), link(QDir(args.dev).filePath("disk/by-partlabel/" + label));
    const QString root = devRoot.canonicalFilePath(), target = link.canonicalFilePath();
    if (root.isEmpty() || target.isEmpty() || !(target == root || target.startsWith(root + '/'))) return {};
    return target;
}
QJsonObject partitionMetadata(const Args &args, const QString &label) {
    const QString path = partitionPath(args, label); if (path.isEmpty()) return {{"present", false}};
    struct stat st{}; if (::stat(QFile::encodeName(path).constData(), &st) != 0) return {{"present", false}};
    const bool block = S_ISBLK(st.st_mode), regular = S_ISREG(st.st_mode);
    if (!block && !(args.allowRegular && regular)) return {{"present", true}, {"readable", false}, {"reason", "unexpected-node-type"}};
    quint64 size = regular ? quint64(st.st_size) : 0;
    if (block) {
        bool ok = false; const quint64 sectors = readText(QDir(args.sys).filePath("class/block/" + QFileInfo(path).fileName() + "/size"), 64).toULongLong(&ok);
        if (ok) size = sectors * 512ULL;
    }
    QJsonObject result{{"present", true}, {"readable", ::access(QFile::encodeName(path).constData(), R_OK) == 0},
                       {"deviceNodeMode", modeString(st.st_mode)}, {"deviceNodeHasWriteBits", bool(st.st_mode & 0222)}};
    if (size) result["sizeBytes"] = qint64(size);
    return result;
}

QByteArray preadExact(int fd, quint64 count, quint64 offset) {
    if (count > MaxDtboSize) fail("read exceeds DTBO bound");
    QByteArray result(qsizetype(count), Qt::Uninitialized); quint64 done = 0;
    while (done < count) {
        const ssize_t amount = ::pread(fd, result.data() + done, size_t(count - done), off_t(offset + done));
        if (amount < 0) { if (errno == EINTR) continue; fail(QStringLiteral("read failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno)))); }
        if (amount == 0) fail(QStringLiteral("short read at offset %1").arg(offset));
        done += quint64(amount);
    }
    return result;
}

void validateFdt(const QByteArray &blob) {
    if (blob.size() < 40 || be32(blob, 0) != FdtMagic) fail("DTBO entry is not a flattened device tree");
    const quint32 total = be32(blob, 4), structure = be32(blob, 8), strings = be32(blob, 12), reserve = be32(blob, 16);
    const quint32 version = be32(blob, 20), compatible = be32(blob, 24), stringSize = be32(blob, 32), structureSize = be32(blob, 36);
    if (total < 40 || total > quint32(blob.size())) fail("FDT total size is out of range");
    if (version < 16 || compatible > version) fail("unsupported FDT version");
    if (reserve < 40 || reserve >= total || reserve % 8 || structure % 4) fail("FDT block alignment is invalid");
    const quint64 structureEnd = addChecked(structure, structureSize, "FDT structure overflow");
    const quint64 stringsEnd = addChecked(strings, stringSize, "FDT strings overflow");
    if (structure < 40 || structureEnd > total || strings < 40 || stringsEnd > total) fail("FDT block is out of range");
    if (qMax(structure, strings) < qMin(structureEnd, stringsEnd)) fail("FDT structure and strings blocks overlap");
    quint64 reserveEnd = 0;
    for (quint64 pos = reserve; pos + 16 <= total; pos += 16) if (be64(blob, pos) == 0 && be64(blob, pos + 8) == 0) { reserveEnd = pos + 16; break; }
    if (!reserveEnd) fail("FDT reservation map has no terminator");
    if (qMax<quint64>(reserve, structure) < qMin(reserveEnd, structureEnd) || qMax<quint64>(reserve, strings) < qMin(reserveEnd, stringsEnd))
        fail("FDT reservation map overlaps another block");
    qint64 depth = 0; bool ended = false;
    for (quint64 pos = structure; pos + 4 <= structureEnd;) {
        const quint32 token = be32(blob, pos); pos += 4;
        if (token == FdtBegin) { const qsizetype end = blob.indexOf('\0', qsizetype(pos)); if (end < 0 || quint64(end) >= structureEnd) fail("unterminated FDT node name"); pos = align4(end + 1); ++depth; }
        else if (token == FdtEndNode) { if (--depth < 0) fail("unbalanced FDT node"); }
        else if (token == FdtProp) {
            if (pos + 8 > structureEnd) fail("truncated FDT property header");
            const quint32 size = be32(blob, pos), name = be32(blob, pos + 4);
            pos += 8;
            if (name >= stringSize || pos + size > structureEnd) fail("FDT property is out of range");
            const qsizetype terminator = blob.indexOf('\0', qsizetype(strings + name)); if (terminator < 0 || quint64(terminator) >= stringsEnd) fail("unterminated FDT property name");
            pos = align4(qsizetype(pos + size));
        } else if (token == FdtNop) continue; else if (token == FdtEnd) { ended = true; break; } else fail("unknown FDT token");
    }
    if (!ended || depth != 0) fail("FDT structure is incomplete");
}

ParsedDtbo parseDtbo(const QString &path, bool allowRegular) {
    struct stat before{}; if (::lstat(QFile::encodeName(path).constData(), &before) != 0) fail("DTBO stat failed");
    if (!S_ISBLK(before.st_mode) && !(allowRegular && S_ISREG(before.st_mode))) fail("DTBO source is not a block device");
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) fail(QStringLiteral("DTBO open failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno))));
    QByteArray raw;
    try {
        const QByteArray header = preadExact(fd, 32, 0);
        if (be32(header, 0) != DtMagic) fail("invalid Android DT table magic");
        const quint32 total = be32(header, 4), headerSize = be32(header, 8), entrySize = be32(header, 12), count = be32(header, 16), offset = be32(header, 20), page = be32(header, 24), version = be32(header, 28);
        if (version != 0 || headerSize != 32 || entrySize != 32) fail("unexpected Android DT table format");
        if (count < 1 || count > MaxDtboEntries || total < 32 || total > MaxDtboSize) fail("Android DT table bounds are invalid");
        if (offset < headerSize || quint64(offset) + quint64(count) * entrySize > total) fail("Android DT table entries are out of range");
        if (page < 512 || page > 65536 || (page & (page - 1))) fail("Android DT table page size is invalid");
        raw = preadExact(fd, total, 0);
    } catch (...) { ::close(fd); throw; }
    ::close(fd);
    const quint32 total = be32(raw, 4), count = be32(raw, 16), entriesOffset = be32(raw, 20), entrySize = be32(raw, 12), page = be32(raw, 24);
    const quint64 entriesEnd = quint64(entriesOffset) + quint64(count) * entrySize; QList<QPair<quint64, quint64>> ranges; QJsonArray entries;
    for (quint32 i = 0; i < count; ++i) {
        const quint64 base = quint64(entriesOffset) + quint64(i) * entrySize; const quint32 size = be32(raw, base), offset = be32(raw, base + 4);
        const quint64 end = quint64(offset) + size; if (size < 40 || offset < entriesEnd || end > total) fail(QStringLiteral("DTBO entry %1 payload is out of range").arg(i));
        for (const auto &range : ranges) if (qMax<quint64>(offset, range.first) < qMin<quint64>(end, range.second)) fail(QStringLiteral("DTBO entry %1 overlaps another payload").arg(i));
        ranges.append({offset, end}); const QByteArray overlay = raw.mid(offset, size); validateFdt(overlay); const QByteArray lower = overlay.toLower();
        QJsonArray features, panels;
        for (const auto &[name, needles] : featureMarkers) {
            bool present = false; for (const auto &needle : needles) present |= lower.contains(needle);
            if (present) { features.append(name); if (name.startsWith("panel-")) panels.append(name.mid(6)); }
        }
        QJsonArray custom; for (int c = 0; c < 4; ++c) custom.append(QStringLiteral("0x%1").arg(be32(raw, base + 16 + c * 4), 8, 16, QLatin1Char('0')));
        entries.append(QJsonObject{{"index", int(i)}, {"sizeBytes", int(size)}, {"id", QStringLiteral("0x%1").arg(be32(raw, base + 8), 8, 16, QLatin1Char('0'))},
                                   {"revision", QStringLiteral("0x%1").arg(be32(raw, base + 12), 8, 16, QLatin1Char('0'))}, {"custom", custom}, {"features", features}, {"panelRevisions", panels}});
    }
    return {{{"format", "android-dtbo-v0"}, {"valid", true}, {"entryCount", int(count)}, {"pageSize", int(page)}, {"totalSizeBytes", int(total)}}, entries, raw};
}

QString requestedSlot(const Args &args) {
    const auto match = QRegularExpression(QStringLiteral("(?:^|\\s)(?:androidboot\\.slot_suffix|androidboot\\.slot|slot_suffix)=_?([ab])(?:\\s|$)" )).match(readText(args.cmdline, 16384));
    return match.hasMatch() ? match.captured(1) : QString{};
}
QString detectPanel(const Args &args) {
    QDirIterator attributes(QDir(args.sys).filePath("bus/mipi-dsi/devices"), {"panel_revision"}, QDir::Files, QDirIterator::Subdirectories);
    while (attributes.hasNext()) { const QString text = readText(attributes.next(), 128).toLower().replace('_', '-'); for (const auto &revision : panelRevisions) if (text.contains(revision)) return revision; }
    QDirIterator compatibles(QDir(args.sys).filePath("firmware/devicetree/base"), {"compatible"}, QDir::Files, QDirIterator::Subdirectories);
    while (compatibles.hasNext()) { const QString path = compatibles.next(); if (!QFileInfo(path).absolutePath().contains("panel@")) continue; const QByteArray data = readBytes(path, 1024).toLower(); for (const auto &revision : panelRevisions) if (data.contains(revision.toLatin1()) || data.contains(QString(revision).replace('-', '_').toLatin1())) return revision; }
    return {};
}

QPair<QJsonObject, QStringList> dtboInventory(const Args &args) {
    QJsonObject partitions; QMap<QString, ParsedDtbo> valid; QStringList failures;
    for (const QString slot : {"a", "b"}) {
        const QString label = "dtbo_" + slot, path = partitionPath(args, label); QJsonObject metadata = partitionMetadata(args, label);
        if (metadata.value("present").toBool() && !path.isEmpty()) try { auto parsed = parseDtbo(path, args.allowRegular); for (auto it = parsed.metadata.begin(); it != parsed.metadata.end(); ++it) metadata[it.key()] = it.value(); valid.insert(slot, std::move(parsed)); }
        catch (const std::exception &error) { metadata["valid"] = false; metadata["error"] = error.what(); failures << label + "-invalid"; }
        partitions[slot] = metadata;
    }
    const QString requested = requestedSlot(args); QString active, reference, confidence = "none", source = "none";
    QString reason = "no Android DTBO partition is present; packaged Linux data remains authoritative";
    const QStringList present = [&]{ QStringList r; for (const QString s : {"a", "b"}) if (partitions.value(s).toObject().value("present").toBool()) r << s; return r; }();
    if (!requested.isEmpty()) { active = requested; if (valid.contains(requested)) { reference = requested; confidence = "boot-chain"; source = "kernel-command-line"; reason = "slot suffix was supplied by the boot chain"; }
        else { confidence = "boot-chain-reference-invalid"; source = "kernel-command-line-rejected"; reason = "the boot-selected slot has no structurally valid DTBO"; failures << "boot-selected-dtbo-unavailable"; } }
    else if (valid.size() == 2 && valid["a"].raw == valid["b"].raw) { reference = "a"; confidence = "content-equivalent"; source = "mirrored-pair"; reason = "A and B DTBO images are byte-identical; active Android slot remains unknown but either image is an equivalent reference"; }
    else if (present.size() == 1 && valid.size() == 1) { reference = valid.firstKey(); confidence = "single-reference-only"; source = "single-partition"; reason = "only one Android DTBO partition exists; it is usable as a reference but does not identify the active Android slot"; }
    else if (!present.isEmpty()) { source = "ambiguous"; reason = "A/B images differ and the boot chain did not publish a trustworthy slot suffix"; failures << "dtbo-slot-ambiguous"; }
    QJsonValue selected;
    if (!reference.isEmpty()) {
        const QString panel = detectPanel(args); QJsonArray compatible;
        const auto entries = valid[reference].entries; for (const auto &value : entries) { const auto entry = value.toObject(); const auto panels = entry.value("panelRevisions").toArray(); bool match = panel.isEmpty() || panels.isEmpty(); for (const auto &p : panels) match |= p.toString() == panel; if (match) compatible.append(entry.value("index")); }
        selected = QJsonObject{{"referenceSlot", reference}, {"entryCount", entries.size()}, {"entries", entries}, {"runtimePanelRevision", panel.isEmpty() ? QJsonValue() : QJsonValue(panel)},
                               {"panelCompatibleEntryIndices", compatible}, {"applicationCandidateSelected", false}, {"selectionPolicy", "inventory-only; never apply Android overlays at runtime"}};
    }
    QJsonObject decision{{"activeAndroidSlot", active.isEmpty() ? QJsonValue() : QJsonValue(active)}, {"referenceSlot", reference.isEmpty() ? QJsonValue() : QJsonValue(reference)},
                         {"confidence", confidence}, {"source", source}, {"reason", reason}};
    return {{ {"policy", "bounded read-only inventory; no mount and no overlay application"}, {"slotDecision", decision}, {"partitions", partitions}, {"selectedInventory", selected} }, failures};
}

QJsonObject cameraCalibration(const Args &args) {
    QJsonObject sensors;
    const QList<std::tuple<QString, QString, QString>> definitions{{"rear", "nabu-rear-camera-calibration*", "ov13b10.yaml"}, {"front", "nabu-front-camera-calibration*", "ov8856.yaml"}};
    for (const auto &[role, pattern, profileName] : definitions) {
        const QDir dir(QDir(args.sys).filePath("bus/nvmem/devices")); const QFileInfoList providers = dir.entryInfoList({pattern}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name); QJsonObject entry{{"providerCount", providers.size()}};
        if (providers.size() == 1) { struct stat st{}; const QString path = providers.first().filePath() + "/nvmem"; if (::stat(QFile::encodeName(path).constData(), &st) == 0)
            entry = QJsonObject{{"providerCount", 1}, {"provider", providers.first().fileName()}, {"sizeBytes", qint64(st.st_size)}, {"mode", modeString(st.st_mode)}, {"kernelExportReadOnly", !(st.st_mode & 0222)}}; }
        const QString profile = QDir(args.etc).filePath("libcamera/ipa/simple/" + profileName); struct stat profileStat{}; const bool present = ::stat(QFile::encodeName(profile).constData(), &profileStat) == 0 && S_ISREG(profileStat.st_mode);
        entry["generatedProfilePresent"] = present; if (present) { entry["generatedProfileMode"] = modeString(profileStat.st_mode); entry["consumerProfileContainsModuleDigest"] = readBytes(profile, 512).contains("SHA256"); }
        sensors[role] = entry;
    }
    return {{"policy", "kernel NVMEM is inspected only for provider metadata; EEPROM bytes and digests are never exported"}, {"sensors", sensors}};
}

QPair<QJsonObject, QStringList> installedFirmware(const Args &args) {
    QJsonObject payloads; QStringList missing;
    for (const auto &[role, relative] : firmwareFiles) { const QString path = QDir(args.firmware).filePath(relative); struct stat st{}; const bool regular = ::stat(QFile::encodeName(path).constData(), &st) == 0 && S_ISREG(st.st_mode), present = regular && st.st_size > 0;
        QJsonObject item{{"present", present}, {"path", "/usr/lib/firmware/" + relative}}; if (regular) { item["sizeBytes"] = qint64(st.st_size); item["mode"] = modeString(st.st_mode); } if (!present) missing << role; payloads[role] = item; }
    return {payloads, missing};
}
QJsonArray remoteProcessors(const Args &args) {
    QJsonArray result; QDir dir(QDir(args.sys).filePath("class/remoteproc"));
    for (const auto &info : dir.entryInfoList({"remoteproc*"}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) { QString firmware = readText(info.filePath() + "/firmware", 512); if (!QRegularExpression("^[A-Za-z0-9_.@/+-]*$").match(firmware).hasMatch()) firmware = "invalid-sysfs-value"; result.append(QJsonObject{{"name", readText(info.filePath() + "/name", 128)}, {"state", readText(info.filePath() + "/state", 128)}, {"firmware", firmware}}); }
    return result;
}
QString unitActive(const QString &unit, bool live) { if (!live) return "not-checked"; QProcess process; process.start("/usr/bin/systemctl", {"is-active", "--quiet", unit}); if (!process.waitForStarted(2000) || !process.waitForFinished(2000)) { process.kill(); return "unavailable"; } return process.exitCode() == 0 ? "active" : "inactive"; }
QJsonObject androidStorage(const Args &args) {
    QJsonObject groups; for (const auto &[role, labels] : partitionGroups) { QJsonObject entries; for (const auto &label : labels) entries[label] = partitionMetadata(args, label); groups[role] = entries; }
    QJsonObject nv; for (const auto &label : modemNv) nv[label] = QJsonObject{{"present", partitionMetadata(args, label).value("present")}};
    const bool live = QFileInfo(args.sys).canonicalFilePath() == "/sys" && QFileInfo(args.dev).canonicalFilePath() == "/dev";
    return {{"firmwareReferences", QJsonObject{{"policy", "availability and device-node metadata only; contents are never mounted, read, hashed, parsed, or auto-extracted"}, {"partitionGroups", groups}}},
            {"modemNv", QJsonObject{{"policy", "owned by rmtfs; never mounted, parsed, copied, hashed, or logged"}, {"partitions", nv}, {"rmtfsService", unitActive("rmtfs.service", live)}, {"tqftpservService", unitActive("tqftpserv.service", live)}}}};
}
bool dtHasProperty(const Args &args, const QString &fragment) {
    const QSet<QString> names{"local-mac-address", "mac-address", "nvmem-cells", "nvmem-cell-names"}; QDirIterator it(QDir(args.sys).filePath("firmware/devicetree/base"), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) { const QFileInfo info(it.next()); if (info.absolutePath().toLower().contains(fragment) && names.contains(info.fileName())) return true; } return false;
}
QJsonObject radioIdentity(const Args &args) {
    QJsonArray interfaces; QDir dir(QDir(args.sys).filePath("class/net")); const QMap<int, QString> assignments{{0,"permanent"},{1,"random"},{2,"stolen"},{3,"userspace-set"}};
    for (const auto &info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) if (QFileInfo::exists(info.filePath() + "/wireless")) { bool ok = false; const int type = readText(info.filePath() + "/addr_assign_type", 32).toInt(&ok); interfaces.append(QJsonObject{{"interface", info.fileName()}, {"assignmentType", ok ? assignments.value(type, "unknown") : "unknown"}}); }
    return {{"policy", "addresses are neither read nor exported; raw modem NV is never used as an address parser input"}, {"wifi", QJsonObject{{"interfaces", interfaces}, {"factoryIdentityDescribedByDeviceTree", dtHasProperty(args, "wifi@")}}},
            {"bluetooth", QJsonObject{{"factoryIdentityDescribedByDeviceTree", dtHasProperty(args, "bluetooth")}}}, {"factoryAdoptionGate", "requires a documented kernel or firmware identity ABI, validation of address type and uniqueness, and rollback; otherwise retain the existing stable Linux address"}};
}

Args parseArgs(const QStringList &input) {
    Args args; for (int i = 1; i < input.size(); ++i) { const QString item = input[i]; auto value = [&](QString &target){ if (++i >= input.size()) fail("missing option value"); target = input[i]; };
        if (item == "--sys-root") value(args.sys); else if (item == "--dev-root") value(args.dev); else if (item == "--firmware-root") value(args.firmware); else if (item == "--etc-root") value(args.etc); else if (item == "--proc-cmdline") value(args.cmdline); else if (item == "--output") value(args.output);
        else if (item == "--stdout") args.stdoutReport = true; else if (item == "--strict") args.strict = true; else if (item == "--allow-regular-fixtures") args.allowRegular = true; else fail("unknown option: " + item); } return args;
}

QPair<QJsonObject, QStringList> buildReport(const Args &args) {
    QStringList failures; const QStringList compatible = readStringList(QDir(args.sys).filePath("firmware/devicetree/base/compatible")); const bool eligible = compatible.contains("xiaomi,nabu"); if (!eligible) failures << "not-xiaomi-nabu";
    auto [dtbo, dtboFailures] = dtboInventory(args); failures += dtboFailures; auto [firmware, missing] = installedFirmware(args); for (const auto &role : missing) failures << "missing-firmware-" + role;
    QJsonArray compatibles; for (const auto &entry : compatible) compatibles.append(entry); QJsonArray failureArray; QSet<QString> unique(failures.cbegin(), failures.cend()); QStringList sorted(unique.cbegin(), unique.cend()); std::sort(sorted.begin(), sorted.end()); for (const auto &entry : sorted) failureArray.append(entry);
    QJsonObject report{{"schemaVersion", 1}, {"device", QJsonObject{{"model", readText(QDir(args.sys).filePath("firmware/devicetree/base/model"), 256)}, {"compatible", compatibles}, {"eligible", eligible}}},
        {"safetyContract", QJsonObject{{"blockDeviceOpenMode", "O_RDONLY"}, {"blockDeviceContentReadAllowlist", QJsonArray{"dtbo_a", "dtbo_b"}}, {"mountsPerformed", false}, {"overlaysApplied", false}, {"androidFirmwarePartitionContentRead", false}, {"modemNvRead", false}, {"radioAddressesRead", false}, {"cameraEepromPayloadRead", false}}},
        {"cameraCalibration", cameraCalibration(args)}, {"dtboReference", dtbo}, {"linuxFirmware", QJsonObject{{"authority", "packaged root filesystem; also works without Android partitions"}, {"payloads", firmware}, {"remoteProcessors", remoteProcessors(args)}}},
        {"androidStorage", androidStorage(args)}, {"radioIdentity", radioIdentity(args)}, {"strictGateFailures", failureArray}};
    return {report, sorted};
}
void writeReport(const QString &path, const QJsonObject &report) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) fail("output directory could not be created");
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) fail("output could not be opened");
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther)) fail("output permissions could not be set");
    if (file.write(QJsonDocument(report).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) fail("atomic output commit failed");
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try { const Args args = parseArgs(app.arguments()); auto [report, failures] = buildReport(args); writeReport(args.output, report); if (args.stdoutReport) QTextStream(stdout) << QJsonDocument(report).toJson(QJsonDocument::Indented); if (args.strict && !failures.isEmpty()) { QTextStream(stderr) << "nabu-hardware-provenance: strict gate failed: " << failures.join(", ") << '\n'; return 1; } return 0; }
    catch (const std::exception &error) { QTextStream(stderr) << "nabu-hardware-provenance: " << error.what() << '\n'; return 2; }
}
