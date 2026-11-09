#include "HostMetrics.h"

#if defined(Q_OS_MACOS)
#include <mach/mach.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#endif

namespace tg {

HostSample HostMetrics::sample()
{
    HostSample sample;
#if defined(Q_OS_MACOS)
    host_cpu_load_info_data_t load{};
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, reinterpret_cast<host_info_t>(&load), &count) == KERN_SUCCESS) {
        const quint64 user = load.cpu_ticks[CPU_STATE_USER];
        const quint64 system = load.cpu_ticks[CPU_STATE_SYSTEM];
        const quint64 idle = load.cpu_ticks[CPU_STATE_IDLE];
        const quint64 nice = load.cpu_ticks[CPU_STATE_NICE];
        if (hasPrevious_) {
            const quint64 du = user - user_;
            const quint64 ds = system - system_;
            const quint64 di = idle - idle_;
            const quint64 dn = nice - nice_;
            const quint64 total = du + ds + di + dn;
            if (total > 0) {
                sample.cpuValid = true;
                sample.cpuPercent = double(du + ds + dn) * 100.0 / double(total);
            }
        }
        user_ = user;
        system_ = system;
        idle_ = idle;
        nice_ = nice;
        hasPrevious_ = true;
    }
    uint64_t physical = 0;
    size_t len = sizeof(physical);
    vm_statistics64_data_t vm{};
    mach_msg_type_number_t vmCount = HOST_VM_INFO64_COUNT;
    if (sysctlbyname("hw.memsize", &physical, &len, nullptr, 0) == 0 && host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &vmCount) == KERN_SUCCESS && physical > 0) {
        const uint64_t used = (uint64_t(vm.active_count) + vm.wire_count + vm.compressor_page_count) * uint64_t(vm_kernel_page_size);
        sample.memValid = true;
        sample.memPercent = qMin(100.0, double(used) * 100.0 / double(physical));
    }
    struct statfs disk{};
    if (statfs("/", &disk) == 0 && disk.f_blocks > 0) {
        sample.diskValid = true;
        sample.diskPercent = (1.0 - double(disk.f_bavail) / double(disk.f_blocks)) * 100.0;
    }
#else
    sample.label = QStringLiteral("Bu bilgisayar — ölçüm bu platformda yok");
#endif
    return sample;
}

} // namespace tg
