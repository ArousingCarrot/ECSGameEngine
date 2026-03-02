#include "InputBackend.h"

#include <SDL3/SDL.h>

namespace plat {

    namespace {
        inline size_t keyIndex(Key k) {
            return static_cast<size_t>(k);
        }
        inline size_t mouseIndex(MouseButton b) {
            return static_cast<size_t>(b);
        }
        inline Key mapScancode(SDL_Scancode sc) {
            switch (sc) {
            case SDL_SCANCODE_W:       return Key::W;
            case SDL_SCANCODE_A:       return Key::A;
            case SDL_SCANCODE_S:       return Key::S;
            case SDL_SCANCODE_D:       return Key::D;
            case SDL_SCANCODE_Q:       return Key::Q;
            case SDL_SCANCODE_E:       return Key::E;
            case SDL_SCANCODE_R:       return Key::R;
            case SDL_SCANCODE_ESCAPE:  return Key::Escape;
            case SDL_SCANCODE_F1:      return Key::F1;
            default:                   return Key::COUNT;
            }
        }

        inline MouseButton mapMouseButton(uint8_t sdlButton) {
            switch (sdlButton) {
            case SDL_BUTTON_LEFT:   return MouseButton::Left;
            case SDL_BUTTON_RIGHT:  return MouseButton::Right;
            case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
            default:                return MouseButton::Left;
            }
        }
    }

    void InputBackend::pumpEvents(
        SDL_Window* window,
        InputState& state,
        const std::function<void(const SDL_Event&)>& extraCallback)
    {
        (void)window; // reserved for future window-ID filtering

        state.beginFrame();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (extraCallback) {
                extraCallback(ev);
            }

            switch (ev.type) {
            case SDL_EVENT_QUIT:
                state.quitRequested = true;
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                state.quitRequested = true;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                state.windowResized = true;
                state.resizedWidth = ev.window.data1;
                state.resizedHeight = ev.window.data2;
                break;

            case SDL_EVENT_MOUSE_MOTION:
                state.mouseDeltaX += static_cast<int>(ev.motion.xrel);
                state.mouseDeltaY += static_cast<int>(ev.motion.yrel);
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                state.wheelX += static_cast<int>(ev.wheel.x);
                state.wheelY += static_cast<int>(ev.wheel.y);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                MouseButton btn = mapMouseButton(ev.button.button);
                size_t idx = mouseIndex(btn);
                state.mousePressed[idx] = !state.mouseDown[idx];
                state.mouseDown[idx] = true;
                break;
            }

            case SDL_EVENT_MOUSE_BUTTON_UP: {
                MouseButton btn = mapMouseButton(ev.button.button);
                size_t idx = mouseIndex(btn);
                state.mouseReleased[idx] = state.mouseDown[idx];
                state.mouseDown[idx] = false;
                break;
            }

            case SDL_EVENT_KEY_DOWN: {
                Key key = mapScancode(ev.key.scancode);
                if (key != Key::COUNT) {
                    size_t idx = keyIndex(key);
                    if (!state.keyDown[idx]) {
                        state.keyPressed[idx] = true;
                    }
                    state.keyDown[idx] = true;
                }
                break;
            }

            case SDL_EVENT_KEY_UP: {
                Key key = mapScancode(ev.key.scancode);
                if (key != Key::COUNT) {
                    size_t idx = keyIndex(key);
                    state.keyReleased[idx] = state.keyDown[idx];
                    state.keyDown[idx] = false;
                }
                break;
            }

            default:
                break;
            }
        }
    }

}
