#include "Trace.h"
#include "Chrono.h"

#include <unordered_map>

namespace {
	thread_local std::vector<diag::TraceEvent> t_localEvents;
	inline uint32_t thread_id_u32() {
		auto id = std::hash<std::thread::id>{}(std::this_thread::get_id());
		return static_cast<uint32_t>(id);
	}
}

namespace diag {
	void TraceCollector::beginFrame(uint64_t) {

	}

	void TraceCollector::endFrame(uint64_t) {
		std::lock_guard<std::mutex> lk(mtx_);
		aggregated_.reserve(aggregated_.size() + t_localEvents.size());
		aggregated_.insert(aggregated_.end(), t_localEvents.begin(), t_localEvents.end());
		t_localEvents.clear();
	}

	void TraceCollector::record(const TraceEvent& ev) {
		std::lock_guard<std::mutex> lk(mtx_);
		aggregated_.push_back(ev);
	}

	ScopedCpuZone::ScopedCpuZone(const char* name, const char* file, uint32_t line)
		: name_(name), file_(file), line_(line), start_ns_(now_ns()), tid_(thread_id_u32()) {
		t_localEvents.push_back(TraceEvent{ name_, file_, line_, EventType::Begin, start_ns_, tid_ });
	}

	ScopedCpuZone::~ScopedCpuZone() {
		int64_t end_ns = now_ns();
		t_localEvents.push_back(TraceEvent{ name_, file_, line_, EventType::End, end_ns, tid_ });
	}

	void Mark(EventType t, const char* name, const char* file, uint32_t line) {
		t_localEvents.push_back(TraceEvent{ name, file, line, t, now_ns(), thread_id_u32() });
	}
}