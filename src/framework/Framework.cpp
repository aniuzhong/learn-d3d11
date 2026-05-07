#include <framework/Framework.h>
#include <framework/HrCheck.h>

#include <algorithm>
#include <cassert>
#include <chrono>

using Microsoft::WRL::ComPtr;

// =============================================================================
// PIMPL — all Win32 + D3D11 internals hidden from the public header
// =============================================================================

struct Framework::Impl {
    // ---- Window ----
    HINSTANCE    hInstance    = nullptr;
    HWND         hwnd         = nullptr;
    std::wstring title        = L"LearnD3D11";
    int          width        = 800;
    int          height       = 600;
    bool         resizable    = true;
    bool         isRunning    = false;
    bool         isMinimized  = false;

    // ---- D3D11 ----
    FrameworkDesc                        desc;
    ComPtr<ID3D11Device>                 device;
    ComPtr<ID3D11DeviceContext>          context;
    ComPtr<IDXGISwapChain>               swapChain;
    ComPtr<ID3D11RenderTargetView>       backbufferRTV;
    ComPtr<ID3D11Texture2D>              depthStencilTexture;
    ComPtr<ID3D11DepthStencilView>       depthStencilView;
    D3D11_VIEWPORT                       viewport = {};

    // ---- Input ----
    InputState input;
    bool       inputKeysPrev[256]   = {};   // for edge detection
    bool       inputMousePrev[3]    = {};   // for edge detection
    bool       cursorLocked         = false;
    bool       cursorVisible        = true;
    bool       cursorFirstLock      = false; // skip delta on first lock frame

    // ---- Timing ----
    LARGE_INTEGER perfFreq   = {};
    LARGE_INTEGER lastTime   = {};
    float         deltaTime  = 0.0f;
    float         totalTime  = 0.0f;

    // ---- Callbacks ----
    ResizeCallback onResize;

    // ---- Window class (registered once per process) ----
    static constexpr const wchar_t* CLASS_NAME = L"LearnD3D11_Framework";
    static bool                      s_classRegistered;
    static LRESULT CALLBACK          WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // ---- Internal helpers ----
    bool CreateInternalWindow();
    bool CreateD3D11Resources();
    void DestroyD3D11Resources();
    bool ResizeSwapChain(int newWidth, int newHeight);
    void ProcessKeyboardMessage(UINT msg, WPARAM wParam, LPARAM lParam);
};

bool Framework::Impl::s_classRegistered = false;

// =============================================================================
// Window Class Registration (once per process)
// =============================================================================

bool Framework::Impl::CreateInternalWindow()
{
    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc   = Impl::WndProc;
        wc.hInstance     = hInstance;
        wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;  // D3D11 handles clearing
        wc.lpszClassName = CLASS_NAME;
        if (!RegisterClassExW(&wc)) return false;
        s_classRegistered = true;
    }

    DWORD exStyle = 0;
    DWORD style   = WS_OVERLAPPEDWINDOW;
    if (!resizable) {
        style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    }

    RECT r = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&r, style, FALSE);

    hwnd = CreateWindowExW(
        exStyle, CLASS_NAME, title.c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, hInstance, this);

    if (!hwnd) return false;

    ShowWindow(hwnd, SW_SHOW);
    return true;
}

// =============================================================================
// Static Window Procedure
// =============================================================================

LRESULT CALLBACK Framework::Impl::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Framework::Impl* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<Framework::Impl*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Framework::Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_SIZE: {
        self->width  = LOWORD(lParam);
        self->height = HIWORD(lParam);
        self->isMinimized = (wParam == SIZE_MINIMIZED);
        if (wParam != SIZE_MINIMIZED && self->swapChain) {
            self->ResizeSwapChain(self->width, self->height);
        }
        if (self->onResize) {
            self->onResize(self->width, self->height);
        }
        return 0;
    }

    case WM_CLOSE:
        self->isRunning = false;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    // Keyboard — track raw up/down; edge detection is done in PollEvents
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        self->ProcessKeyboardMessage(msg, wParam, lParam);
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        self->ProcessKeyboardMessage(msg, wParam, lParam);
        return 0;

    // Mouse move
    case WM_MOUSEMOVE:
        self->input.mouseX = LOWORD(lParam);
        self->input.mouseY = HIWORD(lParam);
        return 0;

    // Mouse buttons
    case WM_LBUTTONDOWN:
        self->input.mouseButtons[0] = true;
        SetCapture(hwnd);
        return 0;
    case WM_LBUTTONUP:
        self->input.mouseButtons[0] = false;
        ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        self->input.mouseButtons[1] = true;
        return 0;
    case WM_RBUTTONUP:
        self->input.mouseButtons[1] = false;
        return 0;
    case WM_MBUTTONDOWN:
        self->input.mouseButtons[2] = true;
        return 0;
    case WM_MBUTTONUP:
        self->input.mouseButtons[2] = false;
        return 0;

    // Scroll wheel
    case WM_MOUSEWHEEL:
        self->input.scrollDelta += GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Framework::Impl::ProcessKeyboardMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (wParam >= 256) return;

    bool isDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
    bool wasDown = (lParam & (1 << 30)) != 0;  // bit 30 = previous key state

    if (isDown && !wasDown) {
        input.keys[wParam] = true;     // first press
    } else if (!isDown) {
        input.keys[wParam] = false;    // release
    }
    // if isDown && wasDown: repeat — ignore, key already true
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

Framework::Framework(const FrameworkDesc& desc)
    : m(new Impl)
{
    m->desc      = desc;
    m->title     = desc.title;
    m->width     = desc.width;
    m->height    = desc.height;
    m->resizable = desc.resizable;
}

Framework::~Framework()
{
    Shutdown();
    delete m;
}

// =============================================================================
// Initialize
// =============================================================================

bool Framework::Initialize(HINSTANCE hInstance)
{
    if (!hInstance) {
        hInstance = GetModuleHandleW(nullptr);
    }
    m->hInstance = hInstance;

    if (!m->CreateInternalWindow()) return false;

    if (!m->CreateD3D11Resources()) return false;

    m->isRunning = true;

    QueryPerformanceFrequency(&m->perfFreq);
    QueryPerformanceCounter(&m->lastTime);

    return true;
}

void Framework::Shutdown()
{
    m->isRunning = false;

    // Unbind resources before destroying
    if (m->context) {
        m->context->OMSetRenderTargets(0, nullptr, nullptr);
        m->context->ClearState();
        m->context->Flush();
    }

    m->DestroyD3D11Resources();

    if (m->hwnd) {
        DestroyWindow(m->hwnd);
        m->hwnd = nullptr;
    }
}

// =============================================================================
// D3D11 Device / SwapChain creation
// =============================================================================

bool Framework::Impl::CreateD3D11Resources()
{
    DestroyD3D11Resources();

    // Feature level: try 11.1→11.0→10.1→10.0→9.3
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
    };
    D3D_FEATURE_LEVEL chosenLevel;

    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (desc.debugDevice) {
        createFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferDesc.Width                   = width;
    sd.BufferDesc.Height                  = height;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format                  = desc.backBufferFormat;
    sd.SampleDesc.Count                   = std::max(1, desc.msaaSamples);
    sd.SampleDesc.Quality                 = desc.msaaSamples > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount                        = desc.bufferCount;
    sd.OutputWindow                       = hwnd;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags                              = 0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                            // default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                            // no software rasterizer
        createFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &sd,
        swapChain.GetAddressOf(),
        device.GetAddressOf(),
        &chosenLevel,
        context.GetAddressOf());

    if (FAILED(hr)) {
        // Fallback: try without debug layer
        if (desc.debugDevice) {
            createFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
                featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
                &sd, swapChain.GetAddressOf(), device.GetAddressOf(),
                &chosenLevel, context.GetAddressOf());
        }
    }
    if (!D3dHrOk(hr, "D3D11CreateDeviceAndSwapChain")) {
        return false;
    }

    return ResizeSwapChain(width, height);
}

void Framework::Impl::DestroyD3D11Resources()
{
    backbufferRTV.Reset();
    depthStencilView.Reset();
    depthStencilTexture.Reset();
    swapChain.Reset();
    context.Reset();
    device.Reset();
}

// =============================================================================
// ResizeSwapChain — destroy & re-create size-dependent resources
// =============================================================================

bool Framework::Impl::ResizeSwapChain(int newWidth, int newHeight)
{
    if (!swapChain || !device || !context) return false;
    if (newWidth <= 0 || newHeight <= 0) return false;

    // Unbind before releasing
    context->OMSetRenderTargets(0, nullptr, nullptr);

    backbufferRTV.Reset();
    depthStencilView.Reset();
    depthStencilTexture.Reset();

    HRESULT hr = swapChain->ResizeBuffers(
        desc.bufferCount,
        newWidth, newHeight,
        desc.backBufferFormat,
        0);

    if (!D3dHrOk(hr, "IDXGISwapChain::ResizeBuffers")) {
        return false;
    }

    // Re-create backbuffer RTV
    ComPtr<ID3D11Texture2D> backBuffer;
    hr = swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (!D3dHrOk(hr, "IDXGISwapChain::GetBuffer(0)")) {
        return false;
    }

    hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr,
                                         backbufferRTV.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateRenderTargetView")) {
        return false;
    }

    // Re-create depth-stencil
    D3D11_TEXTURE2D_DESC dsd = {};
    dsd.Width              = newWidth;
    dsd.Height             = newHeight;
    dsd.MipLevels          = 1;
    dsd.ArraySize          = 1;
    dsd.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsd.SampleDesc.Count   = (std::max)(1, desc.msaaSamples);
    dsd.SampleDesc.Quality = desc.msaaSamples > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
    dsd.Usage              = D3D11_USAGE_DEFAULT;
    dsd.BindFlags          = D3D11_BIND_DEPTH_STENCIL;

    hr = device->CreateTexture2D(&dsd, nullptr,
                                  depthStencilTexture.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateTexture2D (depth stencil)")) {
        return false;
    }

    hr = device->CreateDepthStencilView(depthStencilTexture.Get(), nullptr,
                                         depthStencilView.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateDepthStencilView")) {
        return false;
    }

    // Viewport
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width    = static_cast<float>(newWidth);
    viewport.Height   = static_cast<float>(newHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    width  = newWidth;
    height = newHeight;

    return true;
}

// Public delegation
bool Framework::ResizeSwapChain(int newWidth, int newHeight)
{
    return m->ResizeSwapChain(newWidth, newHeight);
}

// =============================================================================
// PollEvents — message pump + input edge detection + timing
// =============================================================================

void Framework::PollEvents()
{
    // Save previous frame state for edge detection
    memcpy(m->inputKeysPrev, m->input.keys, sizeof(m->input.keys));
    memcpy(m->inputMousePrev, m->input.mouseButtons, sizeof(m->input.mouseButtons));

    // Reset per-frame accumulators
    m->input.scrollDelta = 0;
    m->input.mouseDeltaX = 0;
    m->input.mouseDeltaY = 0;

    // Cursor lock: center cursor & compute delta
    if (m->cursorLocked) {
        POINT center = { m->width / 2, m->height / 2 };
        POINT cursor;
        GetCursorPos(&cursor);
        ScreenToClient(m->hwnd, &cursor);

        if (!m->cursorFirstLock) {
            // First frame after lock: ignore the jump
            m->cursorFirstLock = true;
        } else {
            m->input.mouseDeltaX = cursor.x - center.x;
            m->input.mouseDeltaY = cursor.y - center.y;
        }

        // Recenter
        ClientToScreen(m->hwnd, &center);
        SetCursorPos(center.x, center.y);
        m->input.mouseX = m->width / 2;
        m->input.mouseY = m->height / 2;
    }

    // Process all queued window messages
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) {
            m->isRunning = false;
        }
    }

    // Compute edge transitions for keys
    for (int i = 0; i < 256; ++i) {
        m->input.keysDown[i] = m->input.keys[i] && !m->inputKeysPrev[i];
        m->input.keysUp[i]   = !m->input.keys[i] && m->inputKeysPrev[i];
    }

    // Compute edge transitions for mouse buttons
    for (int i = 0; i < 3; ++i) {
        m->input.mouseButtonsDown[i] = m->input.mouseButtons[i] && !m->inputMousePrev[i];
        m->input.mouseButtonsUp[i]   = !m->input.mouseButtons[i] && m->inputMousePrev[i];
    }

    // Timing
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    m->deltaTime = static_cast<float>(now.QuadPart - m->lastTime.QuadPart)
                   / static_cast<float>(m->perfFreq.QuadPart);
    m->lastTime = now;
    m->totalTime += m->deltaTime;

    // Cap delta to avoid spiral of death after breakpoint / long frame
    if (m->deltaTime > 0.25f) m->deltaTime = 0.25f;
}

// =============================================================================
// BeginFrame / EndFrame
// =============================================================================

void Framework::BeginFrame()
{
    PollEvents();
    ClearBackBuffer(0.2f, 0.3f, 0.3f, 1.0f);
    ClearDepthStencil();
}

void Framework::EndFrame()
{
    SwapBuffers();
}

// =============================================================================
// Clear & Bind helpers
// =============================================================================

void Framework::ClearBackBuffer(float r, float g, float b, float a)
{
    if (!m->context || !m->backbufferRTV) return;

    const float color[4] = { r, g, b, a };
    m->context->ClearRenderTargetView(m->backbufferRTV.Get(), color);
    m->context->OMSetRenderTargets(1, m->backbufferRTV.GetAddressOf(),
                                    m->depthStencilView.Get());
    m->context->RSSetViewports(1, &m->viewport);
}

void Framework::ClearDepthStencil()
{
    if (!m->context || !m->depthStencilView) return;

    m->context->ClearDepthStencilView(m->depthStencilView.Get(),
                                       D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                       1.0f, 0);
    m->context->OMSetRenderTargets(1, m->backbufferRTV.GetAddressOf(),
                                    m->depthStencilView.Get());
}

void Framework::BindDefaultRenderTargets()
{
    if (!m->context || !m->backbufferRTV) return;

    m->context->OMSetRenderTargets(1, m->backbufferRTV.GetAddressOf(),
                                    m->depthStencilView.Get());
    m->context->RSSetViewports(1, &m->viewport);
}

void Framework::SwapBuffers()
{
    if (m->swapChain) {
        m->swapChain->Present(m->desc.vSync ? 1 : 0, 0);
    }
}

// =============================================================================
// IsRunning / Quit
// =============================================================================

bool Framework::IsRunning() const { return m->isRunning; }

void Framework::Quit() { m->isRunning = false; }

// =============================================================================
// Accessors
// =============================================================================

ID3D11Device*           Framework::GetDevice()           const { return m->device.Get(); }
ID3D11DeviceContext*    Framework::GetContext()          const { return m->context.Get(); }
ID3D11RenderTargetView* Framework::GetBackBufferRTV()    const { return m->backbufferRTV.Get(); }
ID3D11DepthStencilView* Framework::GetDepthStencilView() const { return m->depthStencilView.Get(); }
IDXGISwapChain*         Framework::GetSwapChain()        const { return m->swapChain.Get(); }
HWND                    Framework::GetHwnd()             const { return m->hwnd; }

int   Framework::Width()       const { return m->width; }
int   Framework::Height()      const { return m->height; }
float Framework::AspectRatio() const { return static_cast<float>(m->width) / static_cast<float>(m->height); }

// =============================================================================
// Timing
// =============================================================================

float Framework::DeltaTime() const { return m->deltaTime; }
float Framework::TotalTime() const { return m->totalTime; }

// =============================================================================
// Input
// =============================================================================

const InputState& Framework::Input() const { return m->input; }

bool Framework::KeyDown(int vk)    const { return (vk >= 0 && vk < 256) && m->input.keys[vk]; }
bool Framework::KeyPressed(int vk) const { return (vk >= 0 && vk < 256) && m->input.keysDown[vk]; }
bool Framework::KeyReleased(int vk)const { return (vk >= 0 && vk < 256) && m->input.keysUp[vk]; }

bool Framework::MouseDown(int b)    const { return (b >= 0 && b < 3) && m->input.mouseButtons[b]; }
bool Framework::MousePressed(int b) const { return (b >= 0 && b < 3) && m->input.mouseButtonsDown[b]; }

int Framework::MouseX()      const { return m->input.mouseX; }
int Framework::MouseY()      const { return m->input.mouseY; }
int Framework::MouseDeltaX() const { return m->input.mouseDeltaX; }
int Framework::MouseDeltaY() const { return m->input.mouseDeltaY; }
int Framework::ScrollDelta() const { return m->input.scrollDelta; }

// =============================================================================
// Cursor
// =============================================================================

void Framework::SetCursorLocked(bool locked)
{
    if (locked == m->cursorLocked) return;
    m->cursorLocked = locked;

    if (locked) {
        m->cursorFirstLock = false;
        SetCursorVisible(false);
    } else {
        // Restore cursor to last known position
        POINT p = { m->input.mouseX, m->input.mouseY };
        ClientToScreen(m->hwnd, &p);
        SetCursorPos(p.x, p.y);
        SetCursorVisible(true);
    }
}

bool Framework::IsCursorLocked() const { return m->cursorLocked; }

void Framework::SetCursorVisible(bool visible)
{
    m->cursorVisible = visible;
    ShowCursor(visible);
}

// =============================================================================
// Callbacks
// =============================================================================

void Framework::SetResizeCallback(ResizeCallback cb)
{
    m->onResize = std::move(cb);
}
