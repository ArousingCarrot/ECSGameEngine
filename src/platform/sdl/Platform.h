#pragma once

#include <cstdint>

struct SDL_Window;

namespace plat {

    struct InitParams {
        bool enableVideo = true;
        bool enableEvents = true;
        bool enableAudio = false;
    };

    bool Init(const InitParams& params);
    void Shutdown();

    using Ticks = std::uint64_t;

    Ticks  GetTicks();
    double TicksToSeconds(Ticks dt);
    double GetTimeSeconds();

}
