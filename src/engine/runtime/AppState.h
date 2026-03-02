#pragma once
#include <atomic>

namespace AppState {
    inline std::atomic<bool> Paused{ false };
    inline std::atomic<bool> ShouldQuit{ false };
}
