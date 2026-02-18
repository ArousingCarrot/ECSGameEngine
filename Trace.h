#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <thread>

namespace diag {
	enum class EventType : uint8_t {Begin, End, Instant};
	struct TraceEvent {
		const char* name;
		const char* file;
		uint32_t line;
		EventType type;
		int64_t ts_ns;
		uint32_t tid;
	};

	class TraceCollector {
	public:
		void beginFrame(uint64_t frameIdx);
		void endFrame(uint64_t frameIdx);
		void record(const TraceEvent& ev);

		const std::vector<TraceEvent>& events() const {return aggregated_;}
		void clear() { aggregated_.clear(); }

	private:
		std::mutex mtx_;
		std::vector<TraceEvent> aggregated_;
	};

	class ScopedCpuZone {
	public:
		ScopedCpuZone(const char* name, const char* file, uint32_t line);
		~ScopedCpuZone();
	private:
		const char* name_;
		const char* file_;
		uint32_t line_;
		int64_t start_ns_;
		uint32_t tid_;
	};

	void Mark(EventType t, const char* name, const char* file, uint32_t line);
}