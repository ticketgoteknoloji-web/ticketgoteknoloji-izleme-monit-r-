#include "ReportWriter.h"
#include "ReportStats.h"

#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QPdfWriter>
#include <QTextStream>

namespace tg {
namespace {

QString ratioText(const Overview& overview)
{
    if (!overview.healthRatio) return QStringLiteral("Hesaplanamadı (ölçülen servis yok)");
    return QString::number(*overview.healthRatio * 100.0, 'f', 1) + QStringLiteral("%");
}

bool included(const QString& kind, const ModuleSnapshot& module)
{
    if (kind == QLatin1String("gochat")) return module.group == QLatin1String("gochat");
    if (kind == QLatin1String("ticketgo")) return module.group == QLatin1String("ticket-go");
    if (kind == QLatin1String("integrations")) return module.group == QLatin1String("custom");
    if (kind == QLatin1String("incidents")) return false;
    return true;
}

} // namespace

bool writeCsvReport(const QString& path, const QString& kind, const Overview& overview, const QList<ModuleSnapshot>& modules, const QList<Incident>& incidents, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QStringLiteral("CSV yazılamadı.");
        return false;
    }
    QTextStream out(&file);
    out << "alan,deger\n";
    out << "rapor," << kind << "\n";
    out << "saglik_orani," << ratioText(overview) << "\n";
    out << "not,Baglanmamis servisler saglikli sayilmadi\n";
    out << "modul,grup,durum,gecikme_ms,baglanti,kanit\n";
    for (const ModuleSnapshot& module : modules) {
        if (!included(kind, module) && kind != QLatin1String("incidents")) continue;
        if (kind == QLatin1String("incidents")) break;
        out << '"' << module.name << "\",\"" << module.group << "\",\"" << statusLabel(module.status) << "\"," << module.latencyMs << ",\"" << module.connection << "\",\"" << redactText(module.evidence).replace('"', '\'') << "\"\n";
    }
    out << "olay,uygulama,modul,siddet,durum,tekrar,ilk,son,neden\n";
    for (const Incident& incident : incidents) {
        out << incident.id << ",\"" << incident.application << "\",\"" << incident.moduleName << "\"," << incident.severity << "," << incident.state << "," << incident.repeatCount << "," << incident.firstSeen.toString(Qt::ISODate) << "," << incident.lastSeen.toString(Qt::ISODate) << ",\"" << redactText(incident.causeText).replace('"', '\'') << "\"\n";
    }
    if (modules.isEmpty() && incidents.isEmpty()) {
        out << "veri,kayitli olcum yok\n";
    }
    return true;
}

bool writePdfReport(const QString& path, const QString& kind, const Overview& overview, const QList<ModuleSnapshot>& modules, const QList<Incident>& incidents, QString* error)
{
    QPdfWriter writer(path);
    writer.setResolution(96);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setTitle(QStringLiteral("TICKET GO sistem izleme raporu"));
    QPainter painter(&writer);
    if (!painter.isActive()) {
        if (error) *error = QStringLiteral("PDF oluşturulamadı.");
        return false;
    }
    painter.setFont(QFont(QStringLiteral("Helvetica"), 16));
    int y = 48;
    painter.drawText(40, y, QStringLiteral("TICKET GO TEKNOLOJİ AŞ."));
    y += 28;
    painter.setFont(QFont(QStringLiteral("Helvetica"), 11));
    painter.drawText(40, y, QStringLiteral("Sistem İzleme ve Takip Görüntüleme Sistemi"));
    y += 24;
    painter.drawText(40, y, QStringLiteral("Rapor: ") + kind + QStringLiteral("  Sağlık: ") + ratioText(overview));
    y += 22;
    painter.drawText(40, y, QStringLiteral("Bağlanmamış servisler sağlıklı sayılmaz. Gizli alanlar yazılmaz."));
    y += 28;
    int rows = 0;
    for (const ModuleSnapshot& module : modules) {
        if (kind != QLatin1String("incidents") && !included(kind, module)) continue;
        if (kind == QLatin1String("incidents")) break;
        painter.drawText(40, y, module.name + QStringLiteral(" — ") + statusLabel(module.status));
        y += 18;
        if (++rows > 28) break;
    }
    for (const Incident& incident : incidents) {
        if (y > 10000) break;
        painter.drawText(40, y, incident.moduleName + QStringLiteral(" / ") + incident.severity + QStringLiteral(" / ") + incident.state);
        y += 16;
    }
    if (rows == 0 && incidents.isEmpty()) {
        painter.drawText(40, y, QStringLiteral("Kayıtlı ölçüm yok."));
    }
    painter.end();
    return true;
}

bool writeCsvDocument(const QString& path, const QString& title, const QStringList& lines, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QStringLiteral("CSV yazılamadı.");
        return false;
    }
    file.write("\xEF\xBB\xBF");
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "baslik,deger\n";
    out << csvCell(QStringLiteral("rapor")) << "," << csvCell(title) << "\n";
    for (const QString& line : lines) {
        const int split = line.indexOf(QLatin1Char(':'));
        const QString key = split < 0 ? QStringLiteral("satir") : line.left(split);
        const QString value = split < 0 ? line : line.mid(split + 1).trimmed();
        out << csvCell(key) << "," << csvCell(value) << "\n";
    }
    return true;
}

bool writePdfDocument(const QString& path, const QString& title, const QString& period, const QStringList& lines, QString* error)
{
    QPdfWriter writer(path);
    writer.setResolution(120);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setTitle(title);
    QPainter painter(&writer);
    if (!painter.isActive()) {
        if (error) *error = QStringLiteral("PDF oluşturulamadı.");
        return false;
    }
    const QFont titleFont(QStringLiteral(".AppleSystemUIFont"), 16);
    const QFont bodyFont(QStringLiteral(".AppleSystemUIFont"), 10);
    int page = 1;
    auto header = [&]() {
        painter.setFont(titleFont);
        painter.drawText(QRect(40, 36, writer.width() - 80, 28), Qt::AlignLeft, QStringLiteral("TICKET GO TEKNOLOJİ AŞ."));
        painter.setFont(bodyFont);
        painter.drawText(QRect(40, 68, writer.width() - 80, 22), Qt::AlignLeft, title);
        painter.drawText(QRect(40, 90, writer.width() - 80, 22), Qt::AlignLeft, period);
        painter.drawText(QRect(40, 112, writer.width() - 80, 22), Qt::AlignLeft, QStringLiteral("Kaynak: yerel izleme ölçümü. Gizli alan yazılmaz."));
    };
    auto footer = [&]() {
        painter.setFont(bodyFont);
        painter.drawText(QRect(40, writer.height() - 36, writer.width() - 80, 20), Qt::AlignRight, QStringLiteral("Sayfa ") + QString::number(page));
    };
    header();
    int y = 150;
    painter.setFont(bodyFont);
    for (const QString& line : lines) {
        if (y > writer.height() - 70) {
            footer();
            writer.newPage();
            ++page;
            header();
            y = 150;
        }
        painter.drawText(QRect(40, y, writer.width() - 80, 36), Qt::TextWordWrap, line);
        y += 28;
    }
    footer();
    painter.end();
    return QFileInfo(path).isFile() && QFileInfo(path).size() > 0;
}

} // namespace tg
