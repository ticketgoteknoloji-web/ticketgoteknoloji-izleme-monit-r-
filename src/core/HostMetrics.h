#pragma once

#include "OpsTypes.h"

namespace tg {

class HostMetrics {
public:
    HostSample sample();

private:
    bool hasPrevious_ = false;
    quint64 user_ = 0;
    quint64 system_ = 0;
    quint64 idle_ = 0;
    quint64 nice_ = 0;
};

} // namespace tg
