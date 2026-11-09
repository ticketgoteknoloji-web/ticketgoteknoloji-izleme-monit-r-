#pragma once

#include <QString>

namespace tg {

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool valid = false;
};

SemVer parseSemVer(const QString& text);
int compareSemVer(const SemVer& left, const SemVer& right);

enum class VersionOrder { Older, Same, Newer, Invalid };
VersionOrder compareVersions(const QString& current, const QString& remote);

} // namespace tg
