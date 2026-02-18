#pragma once

#include <string>
#include <SDL3/SDL.h>

class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    int getWidth() const;
    int getHeight() const;
    std::string getTitle() const;
    SDL_Window* getSDLWindow() const;

    SDL_GLContext getGLContext() const noexcept { return glContext; }

    void setTitle(const std::string& title);
    void setTitle(const char* title) { setTitle(std::string(title ? title : "")); }

    void setSize(int width, int height);

    bool isInitialized() const;

    void clear();
    void update();
    void close();

    static bool SetSwapInterval(int interval);
    static bool GetSwapInterval(int& outInterval);

    bool setVSync(bool enabled);
    bool getVSync(bool& outEnabled) const;

    void swapBuffers() { update(); }

    static Window* Primary() noexcept { return s_primaryWindow; }

private:
    SDL_Window* window = nullptr;
    SDL_GLContext glContext = nullptr;

    std::string windowTitle;
    int windowWidth = 0;
    int windowHeight = 0;
    bool initialized = false;

    static Window* s_primaryWindow;
};
