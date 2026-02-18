#include "GpuTimers.h"
#include "GraphicsGL.h"
#include <vector>
#include <cassert>

namespace diag {

    static GpuTimerQueryPool* g_pool = nullptr;

    static bool has_timer_query_support() {
#if defined(DIAG_GL_USE_GLAD)
#ifdef GL_ARB_timer_query
        return glad_glQueryCounter != nullptr;
#else
        return glad_glQueryCounter != nullptr;
#endif
#else
        return false;
#endif
    }

    bool GpuTimerQueryPool::initialize() {
        supported_ = has_timer_query_support();
        freeIds_.reserve(256);
        return supported_;
    }

    void GpuTimerQueryPool::beginFrame() {
        frameResults_.clear();
        // activeStack_ should be empty between frames
    }

    void GpuTimerQueryPool::endFrame() {
        if (!supported_) return;

        // Balanced scopes per frame.
        assert(activeStack_.empty());

        // Convert prev-frame query objects to timestamp ranges
        frameResults_.reserve(prevFrameScopes_.size());
        for (auto& s : prevFrameScopes_) {
            GLuint64 start = 0, end = 0;
#if defined(DIAG_GL_USE_GLAD)
            glGetQueryObjectui64v(s.start, GL_QUERY_RESULT, &start);
            glGetQueryObjectui64v(s.end, GL_QUERY_RESULT, &end);
#else
            // Should not get here; supported_ would be false
            (void)s; (void)start; (void)end;
#endif
            frameResults_.push_back({ s.name, static_cast<uint64_t>(start), static_cast<uint64_t>(end) });
            freeQuery(s.start);
            freeQuery(s.end);
        }
        prevFrameScopes_.clear();
    }

    GLQueryID GpuTimerQueryPool::allocQuery() {
        if (!freeIds_.empty()) { auto id = freeIds_.back(); freeIds_.pop_back(); return id; }
        GLQueryID id = 0;
#if defined(DIAG_GL_USE_GLAD)
        glGenQueries(1, &id);
#endif
        return id;
    }

    void GpuTimerQueryPool::freeQuery(GLQueryID q) {
        freeIds_.push_back(q);
    }

    void GpuTimerQueryPool::beginScope(const char* name) {
        if (!supported_) return;
        GLQueryID qStart = allocQuery();
#if defined(DIAG_GL_USE_GLAD)
        glQueryCounter(qStart, GL_TIMESTAMP);
#endif
        activeStack_.push_back({ name, qStart, 0 });
    }

    void GpuTimerQueryPool::endScope() {
        if (!supported_) return;
        assert(!activeStack_.empty());
        auto s = activeStack_.back(); activeStack_.pop_back();
        GLQueryID qEnd = allocQuery();
#if defined(DIAG_GL_USE_GLAD)
        glQueryCounter(qEnd, GL_TIMESTAMP);
#endif
        s.end = qEnd;
        // Defer readback to next endFrame()
        prevFrameScopes_.push_back(s);
    }

    // RAII
    ScopedGpuZone::ScopedGpuZone(const char* name) : name_(name) {
        if (g_pool) g_pool->beginScope(name_);
    }
    ScopedGpuZone::~ScopedGpuZone() {
        if (g_pool) g_pool->endScope();
    }

    bool GpuTimingSupported() { return has_timer_query_support(); }
    static GpuTimerQueryPool g_staticPool;
    bool BindGlobalGpuPool() {
        g_pool = &g_staticPool;
        return g_pool->initialize();
    }
    GpuTimerQueryPool& GetGlobalGpuPool() { return g_staticPool; }

}
