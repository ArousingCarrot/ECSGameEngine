#pragma once

#include "InputState.h"

#include <functional>

struct SDL_Window;
union SDL_Event;

namespace plat {

    class InputBackend {
    public:
        void pumpEvents(
            SDL_Window* window,
            InputState& state,
            const std::function<void(const SDL_Event&)>& extraCallback = {});
    };

}
