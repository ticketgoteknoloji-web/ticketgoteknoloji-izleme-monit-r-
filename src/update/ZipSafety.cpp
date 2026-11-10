#include "ZipSafety.h"

#include "UpdateTypes.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

namespace tg {
namespace {

quint16 read16(const QByteArray& bytes, int offset)
{
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.constData() + offset);
    return static_cast<quint16>(data[0] | (data[1] << 8));
}

quint32 read32(const QByteArray& bytes, int offset)
{
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.constData() + offset);
    return static_cast<quint32>(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
}

bool unsafeName(const QString& name)
{
    if (name.isEmpty() || name.startsWith(QLatin1Char('/')) || name.startsWith(QLatin1Char('\\'))) return true;
    if (name.contains(QLatin1Char('\\')) || name.contains(QChar('\0'))) return true;
    const QStringList parts = name.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return true;
    for (const QString& part : parts) {
        if (part == QLatin1String("..") || part == QLatin1String(".")) return true;
    }
    return false;
}

} // namespace

ZipInspection inspectZip(const QString& zipPath)
{
    ZipInspection inspection;
    QFile file(zipPath);
    if (!file.open(QIODevice::ReadOnly)) {
        inspection.message = msgPathRejected();
        return inspection;
    }
    const qint64 fileSize = file.size();
    if (fileSize < 22 || fileSize > 1024LL * 1024LL * 1024LL) {
        inspection.message = QStringLiteral("Paket boyutu beklenen aralıkta değil.");
        return inspection;
    }
    const int tailSize = static_cast<int>(qMin<qint64>(fileSize, 66000));
    file.seek(fileSize - tailSize);
    const QByteArray tail = file.read(tailSize);
    int eocd = -1;
    for (int index = tail.size() - 22; index >= 0; --index) {
        if (read32(tail, index) == 0x06054b50u) {
            eocd = index;
            break;
        }
    }
    if (eocd < 0) {
        inspection.message = QStringLiteral("Paket ZIP yapısı okunamadı.");
        return inspection;
    }
    const quint16 count = read16(tail, eocd + 10);
    const quint32 cdSize = read32(tail, eocd + 12);
    const quint32 cdOffset = read32(tail, eocd + 16);
    if (cdOffset == 0xffffffffu || cdSize == 0xffffffffu || cdSize > 32u * 1024u * 1024u) {
        inspection.message = QStringLiteral("ZIP64 paketi doğrulanamadı.");
        return inspection;
    }
    if (static_cast<qint64>(cdOffset) + cdSize > fileSize) {
        inspection.message = QStringLiteral("Paket ZIP yapısı okunamadı.");
        return inspection;
    }
    file.seek(cdOffset);
    const QByteArray bytes = file.read(cdSize);
    if (bytes.size() != static_cast<int>(cdSize)) {
        inspection.message = QStringLiteral("Paket ZIP yapısı okunamadı.");
        return inspection;
    }
    int cursor = 0;
    QString appName;
    for (quint16 entry = 0; entry < count; ++entry) {
        if (cursor + 46 > bytes.size() || read32(bytes, cursor) != 0x02014b50u) {
            inspection.message = QStringLiteral("Paket ZIP yapısı okunamadı.");
            return inspection;
        }
        const quint16 flags = read16(bytes, cursor + 8);
        const quint16 nameLen = read16(bytes, cursor + 28);
        const quint16 extraLen = read16(bytes, cursor + 30);
        const quint16 commentLen = read16(bytes, cursor + 32);
        const quint32 external = read32(bytes, cursor + 38);
        if (cursor + 46 + nameLen > bytes.size()) {
            inspection.message = QStringLiteral("Paket ZIP yapısı okunamadı.");
            return inspection;
        }
        const QByteArray raw = bytes.mid(cursor + 46, nameLen);
        const QString name = (flags & 0x800) ? QString::fromUtf8(raw) : QString::fromLocal8Bit(raw);
        if (unsafeName(name)) {
            inspection.message = msgPathRejected();
            return inspection;
        }
        const quint32 mode = external >> 16;
        if ((mode & 0170000) == 0120000) {
            inspection.message = msgPathRejected();
            return inspection;
        }
        inspection.names.append(name);
        if (name.endsWith(QStringLiteral(".app")) || name.contains(QStringLiteral(".app/"))) {
            const QString top = name.section(QLatin1Char('/'), 0, 0);
            if (!top.endsWith(QStringLiteral(".app"))) {
                inspection.message = msgPathRejected();
                return inspection;
            }
            if (appName.isEmpty()) appName = top;
            else if (appName != top) {
                inspection.message = QStringLiteral("Pakette birden fazla uygulama var.");
                return inspection;
            }
        }
        cursor += 46 + nameLen + extraLen + commentLen;
    }
    if (appName.isEmpty()) {
        inspection.message = QStringLiteral("Pakette macOS uygulaması yok.");
        return inspection;
    }
    inspection.ok = true;
    inspection.appName = appName;
    return inspection;
}

bool extractZip(const QString& zipPath, const QString& destination, QString* error)
{
    const ZipInspection inspection = inspectZip(zipPath);
    if (!inspection.ok) {
        if (error) *error = inspection.message;
        return false;
    }
    QDir().mkpath(destination);
    QProcess process;
#if defined(Q_OS_MACOS)
    process.start(QStringLiteral("/usr/bin/ditto"), {QStringLiteral("-x"), QStringLiteral("-k"), zipPath, destination});
#else
    // Production Windows updates use the Setup.exe path; this branch keeps macOS package tests runnable on Windows CI.
    process.start(QStringLiteral("tar"), {QStringLiteral("-xf"), zipPath, QStringLiteral("-C"), destination});
#endif
    if (!process.waitForFinished(120000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) *error = QStringLiteral("Paket açılamadı.");
        return false;
    }
    return treeStaysInside(destination, error);
}

bool treeStaysInside(const QString& root, QString* error)
{
    const QString canonicalRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    QDirIterator iterator(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo info = iterator.fileInfo();
        if (info.isSymLink()) {
            const QString target = QDir::cleanPath(QFileInfo(info.symLinkTarget()).absoluteFilePath());
            if (target != canonicalRoot && !target.startsWith(canonicalRoot + QLatin1Char('/')) && !target.startsWith(canonicalRoot + QLatin1Char('\\'))) {
                if (error) *error = msgPathRejected();
                return false;
            }
        }
        const QString path = QDir::cleanPath(info.absoluteFilePath());
        if (path != canonicalRoot && !path.startsWith(canonicalRoot + QLatin1Char('/')) && !path.startsWith(canonicalRoot + QLatin1Char('\\'))) {
            if (error) *error = msgPathRejected();
            return false;
        }
    }
    return true;
}

QString findSingleApp(const QString& root, QString* error)
{
    const QDir directory(root);
    const QStringList apps = directory.entryList({QStringLiteral("*.app")}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (apps.size() != 1) {
        if (error) *error = apps.isEmpty() ? QStringLiteral("Pakette macOS uygulaması yok.") : QStringLiteral("Pakette birden fazla uygulama var.");
        return {};
    }
    return directory.filePath(apps.first());
}

} // namespace tg
