#include "Platform.h"

#include <SDL3/SDL.h>
#include <iostream>

namespace plat {

    static std::uint64_t gPerfFreq = 0;
    static std::uint64_t gStartTick = 0;
    static bool          gInitialized = false;

    bool Init(const InitParams& params) {
        if (gInitialized) {
            return true;
        }

        Uint32 sdlFlags = 0;
        if (params.enableVideo)  sdlFlags |= SDL_INIT_VIDEO;
        if (params.enableEvents) sdlFlags |= SDL_INIT_EVENTS;
        if (params.enableAudio)  sdlFlags |= SDL_INIT_AUDIO;

        if (SDL_Init(sdlFlags) != 0) {
            std::cerr << "[Platform] SDL_Init failed: "
                << (SDL_GetError() ? SDL_GetError() : "(no message)") << "\n";
            return false;
        }

        gPerfFreq = SDL_GetPerformanceFrequency();
        gStartTick = SDL_GetPerformanceCounter();
        gInitialized = true;
        return true;
    }

    void Shutdown() {
        if (!gInitialized) return;
        SDL_Quit();
        gInitialized = false;
    }

    Ticks GetTicks() {
        return SDL_GetPerformanceCounter();
    }

    double TicksToSeconds(Ticks dt) {
        if (gPerfFreq == 0) {
            gPerfFreq = SDL_GetPerformanceFrequency();
        }
        return static_cast<double>(dt) / static_cast<double>(gPerfFreq);
    }

    double GetTimeSeconds() {
        if (!gInitialized) return 0.0;
        Ticks now = SDL_GetPerformanceCounter();
        return TicksToSeconds(now - gStartTick);
    }

}
