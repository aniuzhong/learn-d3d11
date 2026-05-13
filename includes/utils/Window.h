#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>
#include <functional>
#include <string>

namespace win32 {

// =============================================================================
// WindowDesc — creation-time configuration
// =============================================================================

struct WindowDesc {
    const wchar_t* title     = L"LearnD3D11";
    int            width     = 800;
    int            height    = 600;
    bool           resizable = true;
};

// =============================================================================
// KeyAction — aligned with GLFW_PRESS / GLFW_RELEASE / GLFW_REPEAT
// =============================================================================

namespace KeyAction {
    constexpr int Release = 0;
    constexpr int Press   = 1;
    constexpr int Repeat  = 2;
}

// =============================================================================
// win32::Window — HWND + input + timing + callbacks
// =============================================================================

class Window {
public:
    using FramebufferSizeCallback = std::function<void(int width, int height)>;
    using KeyCallback             = std::function<void(int key, int scancode, int action, int mods)>;
    using CursorPosCallback       = std::function<void(double xpos, double ypos)>;
    using ScrollCallback          = std::function<void(double xoffset, double yoffset)>;

    Window(const WindowDesc& desc = {});
    ~Window();

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    // ---- Aligned with glfw::Window ----
    HWND Get() const;
    bool ShouldClose() const;
    void SetShouldClose(bool v);
    bool GetKey(int vk) const;
    void GetFramebufferSize(int* width, int* height) const;

    // ---- Extended input ----
    bool GetKeyPressed(int vk) const;
    bool GetKeyReleased(int vk) const;
    bool GetMouseButton(int button) const;
    bool GetMousePressed(int button) const;
    void GetCursorPos(double* xpos, double* ypos) const;
    void GetCursorDelta(double* dx, double* dy) const;
    double GetScrollDelta() const;

    // ---- Cursor ----
    void SetCursorLocked(bool locked);
    bool IsCursorLocked() const;
    void SetCursorVisible(bool visible);

    // ---- Timing ----
    float DeltaTime() const;
    float GetTime() const;

    // ---- Callbacks (public members like glfw::Window) ----
    FramebufferSizeCallback on_framebuffer_size_;
    KeyCallback             on_key_;
    CursorPosCallback       on_cursor_pos_;
    ScrollCallback          on_scroll_;

    // ---- Message pump ----
    void PollEvents();

private:
    static constexpr const wchar_t* CLASS_NAME = L"LearnD3D11_Window";
    static bool                      s_classRegistered;
    static LRESULT CALLBACK          WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void ProcessKeyboardMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // ---- Window ----
    HINSTANCE    hInstance_  = nullptr;
    HWND         hwnd_       = nullptr;
    std::wstring title_      = L"LearnD3D11";
    int          width_      = 800;
    int          height_     = 600;
    bool         resizable_  = true;
    bool         isRunning_  = false;
    bool         isMinimized_= false;

    // ---- Input ----
    bool keys_[256]       = {};
    bool keysPrev_[256]   = {};
    bool keysDown_[256]   = {};
    bool keysUp_[256]     = {};
    int  mouseX_          = 0;
    int  mouseY_          = 0;
    int  mouseDeltaX_     = 0;
    int  mouseDeltaY_     = 0;
    bool mouseButtons_[3]    = {};
    bool mousePrev_[3]       = {};
    bool mouseButtonsDown_[3]= {};
    bool mouseButtonsUp_[3]  = {};
    int  scrollDelta_     = 0;

    // ---- Cursor ----
    bool cursorLocked_    = false;
    bool cursorVisible_   = true;
    bool cursorFirstLock_ = false;

    // ---- Timing ----
    LARGE_INTEGER perfFreq_  = {};
    LARGE_INTEGER lastTime_  = {};
    float         deltaTime_ = 0.0f;
    float         totalTime_ = 0.0f;
};

} // namespace win32
