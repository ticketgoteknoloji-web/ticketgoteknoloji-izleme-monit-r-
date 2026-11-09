#include "SemVer.h"

#include <QRegularExpression>

namespace tg {

SemVer parseSemVer(const QString& text)
{
    SemVer version;
    static const QRegularExpression pattern(QStringLiteral("^v?(\\d+)\\.(\\d+)\\.(\\d+)$"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(text.trimmed());
    if (!match.hasMatch()) return version;
    version.major = match.captured(1).toInt();
    version.minor = match.captured(2).toInt();
    version.patch = match.captured(3).toInt();
    version.valid = true;
    return version;
}

int compareSemVer(const SemVer& left, const SemVer& right)
{
    if (left.major != right.major) return left.major < right.major ? -1 : 1;
    if (left.minor != right.minor) return left.minor < right.minor ? -1 : 1;
    if (left.patch != right.patch) return left.patch < right.patch ? -1 : 1;
    return 0;
}

VersionOrder compareVersions(const QString& current, const QString& remote)
{
    const SemVer left = parseSemVer(current);
    const SemVer right = parseSemVer(remote);
    if (!left.valid || !right.valid) return VersionOrder::Invalid;
    const int order = compareSemVer(right, left);
    if (order > 0) return VersionOrder::Newer;
    if (order < 0) return VersionOrder::Older;
    return VersionOrder::Same;
}

} // namespace tg
