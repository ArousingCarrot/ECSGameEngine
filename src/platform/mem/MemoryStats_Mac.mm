#ifdef __APPLE__
#include "MemoryStats.h"
#include <mach/mach.h>

namespace diag {
bool GetProcessMemory(ProcessMemory& out) {
    task_basic_info_64 info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_64_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO_64, (task_info_t)&info, &count) != KERN_SUCCESS) return false;
    out.rss_bytes = info.resident_size;
    out.peak_bytes = info.resident_size_max;
    return true;
}
}
#endif
