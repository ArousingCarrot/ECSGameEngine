#pragma once

#include "Trace.h"
#include "GpuTimers.h"

#if DIAG_ENABLE
#define ZONE_CPU(name) ::diag::ScopedCpuZone _diag_cpu_zone_##__LINE__{name, __FILE__, __LINE__}
#define MARK(name) ::diag::Mark(::diag::EventType::Instant, name, __FILE__, __LINE__)
#define ZONE_GPU(name) ::diag::ScopedGpuZone _diag_gpu_zone_##__LINE__{name}
#else
#define ZONE_CPU(name) (void)0
#define MARK(name) (void)0
#define ZONE_GPU(name) (void)0
#endif
