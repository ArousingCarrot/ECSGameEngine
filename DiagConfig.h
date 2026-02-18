#pragma once

#ifndef DIAG_ENABLE
#define DIAG_ENABLE 1
#endif

namespace diag {
	enum class ProfilerMode {Off, RollingMinimal, Full};
	struct DiagnosticsConfig {
		ProfilerMode mode = ProfilerMode::RollingMinimal;
		int rollingFrames = 600;
		bool captureGpu = true;
		bool captureCpu = true;
	};
}