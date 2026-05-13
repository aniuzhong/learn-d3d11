#include <utils/Window.h>

namespace win32 {

bool Window::s_classRegistered = false;

// =============================================================================
// Constructor / Destructor
// =============================================================================

Window::Window(const WindowDesc& desc)
{
    title_     = desc.title;
    width_     = desc.width;
    height_    = desc.height;
    resizable_ = desc.resizable;
    hInstance_ = GetModuleHandleW(nullptr);

    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = hInstance_;
        wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = CLASS_NAME;
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    DWORD style = resizable_ ? WS_OVERLAPPEDWINDOW
                             : (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

    RECT r = { 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_) };
    AdjustWindowRect(&r, style, FALSE);

    hwnd_ = CreateWindowExW(
        0, CLASS_NAME, title_.c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, hInstance_, this);

    if (hwnd_) {
        isRunning_ = true;
        ShowWindow(hwnd_, SW_SHOW);
        QueryPerformanceFrequency(&perfFreq_);
        QueryPerformanceCounter(&lastTime_);
    }
}

Window::~Window()
{
    isRunning_ = false;
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

Window::Window(Window&& other) noexcept
{
    *this = std::move(other);
}

Window& Window::operator=(Window&& other) noexcept
{
    if (this != &other) {
        // Move window
        hwnd_       = other.hwnd_;       other.hwnd_       = nullptr;
        hInstance_  = other.hInstance_;  other.hInstance_  = nullptr;
        title_      = std::move(other.title_);
        width_      = other.width_;      other.width_      = 0;
        height_     = other.height_;     other.height_     = 0;
        resizable_  = other.resizable_;
        isRunning_  = other.isRunning_;  other.isRunning_  = false;
        isMinimized_= other.isMinimized_;

        // Move input
        memcpy(keys_,            other.keys_,            sizeof(keys_));
        memcpy(keysPrev_,        other.keysPrev_,        sizeof(keysPrev_));
        memcpy(keysDown_,        other.keysDown_,        sizeof(keysDown_));
        memcpy(keysUp_,          other.keysUp_,          sizeof(keysUp_));
        mouseX_      = other.mouseX_;      other.mouseX_      = 0;
        mouseY_      = other.mouseY_;      other.mouseY_      = 0;
        mouseDeltaX_ = other.mouseDeltaX_; other.mouseDeltaX_ = 0;
        mouseDeltaY_ = other.mouseDeltaY_; other.mouseDeltaY_ = 0;
        memcpy(mouseButtons_,     other.mouseButtons_,     sizeof(mouseButtons_));
        memcpy(mousePrev_,        other.mousePrev_,        sizeof(mousePrev_));
        memcpy(mouseButtonsDown_, other.mouseButtonsDown_, sizeof(mouseButtonsDown_));
        memcpy(mouseButtonsUp_,   other.mouseButtonsUp_,   sizeof(mouseButtonsUp_));
        scrollDelta_ = other.scrollDelta_; other.scrollDelta_ = 0;

        // Move cursor
        cursorLocked_    = other.cursorLocked_;    other.cursorLocked_    = false;
        cursorVisible_   = other.cursorVisible_;
        cursorFirstLock_ = other.cursorFirstLock_;

        // Move timing
        perfFreq_  = other.perfFreq_;
        lastTime_  = other.lastTime_;
        deltaTime_ = other.deltaTime_; other.deltaTime_ = 0.0f;
        totalTime_ = other.totalTime_; other.totalTime_ = 0.0f;

        // Move callbacks
        on_framebuffer_size_ = std::move(other.on_framebuffer_size_);
        on_key_              = std::move(other.on_key_);
        on_cursor_pos_       = std::move(other.on_cursor_pos_);
        on_scroll_           = std::move(other.on_scroll_);

        // Update window user data to point to this
        if (hwnd_)
            SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
    return *this;
}

// =============================================================================
// Static Window Procedure
// =============================================================================

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Window* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_SIZE:
        self->width_      = LOWORD(lParam);
        self->height_     = HIWORD(lParam);
        self->isMinimized_ = (wParam == SIZE_MINIMIZED);
        if (self->on_framebuffer_size_)
            self->on_framebuffer_size_(self->width_, self->height_);
        return 0;

    case WM_CLOSE:
        self->isRunning_ = false;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        self->ProcessKeyboardMessage(msg, wParam, lParam);
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        self->ProcessKeyboardMessage(msg, wParam, lParam);
        return 0;

    case WM_MOUSEMOVE:
        self->mouseX_ = LOWORD(lParam);
        self->mouseY_ = HIWORD(lParam);
        return 0;

    case WM_LBUTTONDOWN:
        self->mouseButtons_[0] = true;
        SetCapture(hwnd);
        return 0;
    case WM_LBUTTONUP:
        self->mouseButtons_[0] = false;
        ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        self->mouseButtons_[1] = true;
        return 0;
    case WM_RBUTTONUP:
        self->mouseButtons_[1] = false;
        return 0;
    case WM_MBUTTONDOWN:
        self->mouseButtons_[2] = true;
        return 0;
    case WM_MBUTTONUP:
        self->mouseButtons_[2] = false;
        return 0;

    case WM_MOUSEWHEEL:
        self->scrollDelta_ += GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Window::ProcessKeyboardMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (wParam >= 256) return;

    bool isDown  = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
    bool wasDown = (lParam & (1 << 30)) != 0;

    if (isDown && !wasDown)
        keys_[wParam] = true;
    else if (!isDown)
        keys_[wParam] = false;
}

// =============================================================================
// PollEvents — message pump + input edge detection + timing
// =============================================================================

void Window::PollEvents()
{
    if (!isRunning_) return;

    memcpy(keysPrev_, keys_, sizeof(keys_));
    memcpy(mousePrev_, mouseButtons_, sizeof(mouseButtons_));

    scrollDelta_ = 0;
    mouseDeltaX_ = 0;
    mouseDeltaY_ = 0;

    if (cursorLocked_) {
        POINT center = { width_ / 2, height_ / 2 };
        POINT cursor;
        ::GetCursorPos(&cursor);
        ScreenToClient(hwnd_, &cursor);

        if (!cursorFirstLock_) {
            cursorFirstLock_ = true;
        } else {
            mouseDeltaX_ = cursor.x - center.x;
            mouseDeltaY_ = cursor.y - center.y;
        }

        ClientToScreen(hwnd_, &center);
        ::SetCursorPos(center.x, center.y);
        mouseX_ = width_ / 2;
        mouseY_ = height_ / 2;
    }

    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT)
            isRunning_ = false;
    }

    for (int i = 0; i < 256; ++i) {
        keysDown_[i] = keys_[i] && !keysPrev_[i];
        keysUp_[i]   = !keys_[i] && keysPrev_[i];
    }

    for (int i = 0; i < 3; ++i) {
        mouseButtonsDown_[i] = mouseButtons_[i] && !mousePrev_[i];
        mouseButtonsUp_[i]   = !mouseButtons_[i] && mousePrev_[i];
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    deltaTime_ = static_cast<float>(now.QuadPart - lastTime_.QuadPart)
                / static_cast<float>(perfFreq_.QuadPart);
    lastTime_ = now;
    totalTime_ += deltaTime_;

    if (deltaTime_ > 0.25f) deltaTime_ = 0.25f;
}

// =============================================================================
// Accessors
// =============================================================================

HWND Window::Get() const                          { return hwnd_; }
bool Window::ShouldClose() const                  { return !isRunning_; }
void Window::SetShouldClose(bool v)               { isRunning_ = !v; }
bool Window::GetKey(int vk) const                 { return (vk >= 0 && vk < 256) && keys_[vk]; }
bool Window::GetKeyPressed(int vk) const          { return (vk >= 0 && vk < 256) && keysDown_[vk]; }
bool Window::GetKeyReleased(int vk) const         { return (vk >= 0 && vk < 256) && keysUp_[vk]; }
void Window::GetFramebufferSize(int* w, int* h) const { if (w) *w = width_; if (h) *h = height_; }
bool Window::GetMouseButton(int b) const          { return (b >= 0 && b < 3) && mouseButtons_[b]; }
bool Window::GetMousePressed(int b) const         { return (b >= 0 && b < 3) && mouseButtonsDown_[b]; }
void Window::GetCursorPos(double* x, double* y) const { if (x) *x = static_cast<double>(mouseX_); if (y) *y = static_cast<double>(mouseY_); }
void Window::GetCursorDelta(double* dx, double* dy) const { if (dx) *dx = static_cast<double>(mouseDeltaX_); if (dy) *dy = static_cast<double>(mouseDeltaY_); }
double Window::GetScrollDelta() const             { return static_cast<double>(scrollDelta_); }

// =============================================================================
// Cursor
// =============================================================================

void Window::SetCursorLocked(bool locked)
{
    if (locked == cursorLocked_) return;
    cursorLocked_ = locked;

    if (locked) {
        cursorFirstLock_ = false;
        SetCursorVisible(false);
    } else {
        POINT p = { mouseX_, mouseY_ };
        ClientToScreen(hwnd_, &p);
        ::SetCursorPos(p.x, p.y);
        SetCursorVisible(true);
    }
}

bool Window::IsCursorLocked() const               { return cursorLocked_; }

void Window::SetCursorVisible(bool visible)
{
    cursorVisible_ = visible;
    ShowCursor(visible);
}

// =============================================================================
// Timing
// =============================================================================

float Window::DeltaTime() const { return deltaTime_; }
float Window::GetTime() const   { return totalTime_; }

} // namespace win32
