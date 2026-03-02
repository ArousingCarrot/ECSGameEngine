#pragma once

#include <chrono>
#include <cstdint>

namespace diag {
	using Clock = std::chrono::steady_clock;

	inline int64_t now_ns() {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			Clock::now().time_since_epoch()).count();
	}
	inline double ns_to_ms(int64_t ns) {
		return double(ns) / 1'000'000.0;
	}
}