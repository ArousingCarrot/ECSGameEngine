#pragma once

#include <array>
#include <cstdint>

namespace plat {

    enum class Key : std::uint8_t {
        W = 0, A, S, D, Q, E,
        R,
        Escape,
        F1,
        COUNT
    };

    enum class MouseButton : std::uint8_t {
        Left = 0,
        Right,
        Middle,
        COUNT
    };

    struct InputState {
        // Keyboard
        std::array<bool, static_cast<size_t>(Key::COUNT)> keyDown{};
        std::array<bool, static_cast<size_t>(Key::COUNT)> keyPressed{};
        std::array<bool, static_cast<size_t>(Key::COUNT)> keyReleased{};

        // Mouse buttons
        std::array<bool, static_cast<size_t>(MouseButton::COUNT)> mouseDown{};
        std::array<bool, static_cast<size_t>(MouseButton::COUNT)> mousePressed{};
        std::array<bool, static_cast<size_t>(MouseButton::COUNT)> mouseReleased{};

        // Mouse motion and wheel
        int mouseDeltaX = 0;
        int mouseDeltaY = 0;
        int wheelX = 0;
        int wheelY = 0;

        // Misc window state
        bool quitRequested = false;
        bool windowResized = false;
        int  resizedWidth = 0;
        int  resizedHeight = 0;

        void beginFrame() noexcept {
            keyPressed.fill(false);
            keyReleased.fill(false);
            mousePressed.fill(false);
            mouseReleased.fill(false);
            mouseDeltaX = 0;
            mouseDeltaY = 0;
            wheelX = 0;
            wheelY = 0;
            windowResized = false;
        }
    };

}
