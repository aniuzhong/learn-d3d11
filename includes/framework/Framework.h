#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>  // Microsoft::WRL::ComPtr

#include <cstdint>
#include <functional>

// =============================================================================
// InputState — per-frame input snapshot, updated by Framework::PollEvents()
// =============================================================================

struct InputState {
    // Keyboard: index by Win32 VK_* code (VK_ESCAPE, VK_W, VK_SPACE, ...)
    bool keys[256]        = {};   // held this frame
    bool keysDown[256]    = {};   // pressed this frame (edge: up→down)
    bool keysUp[256]      = {};   // released this frame (edge: down→up)

    // Mouse position
    int   mouseX          = 0;
    int   mouseY          = 0;
    int   mouseDeltaX     = 0;    // relative motion since last frame
    int   mouseDeltaY     = 0;

    // Mouse buttons: 0=left, 1=right, 2=middle (same as GLFW & Win32)
    bool  mouseButtons[3]    = {};
    bool  mouseButtonsDown[3]= {};
    bool  mouseButtonsUp[3]  = {};

    // Scroll wheel
    int   scrollDelta     = 0;
};

// =============================================================================
// FrameworkDesc — creation-time configuration
// =============================================================================

struct FrameworkDesc {
    // ---- Window ----
    const wchar_t* title      = L"LearnD3D11";
    int            width      = 800;
    int            height     = 600;
    bool           fullscreen = false;
    bool           resizable  = true;

    // ---- D3D11 SwapChain ----
    DXGI_FORMAT    backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;  // or R16G16B16A16_FLOAT for HDR (Ch5.6)
    int            bufferCount      = 2;
    int            msaaSamples      = 1;    // 1=off, 4=4x (Ch4.11), 8=8x
    bool           vSync            = true;

    // ---- D3D11 Device ----
    bool           debugDevice      = false; // D3D11_CREATE_DEVICE_DEBUG
};

// =============================================================================
// Framework — window + D3D11 device/swapchain + input + timing
//
// Usage (simple demo, 95% of cases):
//
//   Framework fw({.title = L"01-HelloTriangle", .width = 800, .height = 600});
//   fw.Initialize();
//   // create shaders, buffers, etc. via fw.GetDevice() / fw.GetContext()
//   while (fw.IsRunning()) {
//       fw.BeginFrame();   // poll input + clear backbuffer & depth
//       // ... draw calls ...
//       fw.EndFrame();     // Present
//   }
//   fw.Shutdown();
//
// Usage (advanced demo with custom render targets):
//
//   while (fw.IsRunning()) {
//       fw.PollEvents();
//       // render to shadow map...
//       fw.ClearBackBuffer(0.1f, 0.1f, 0.1f, 1.0f);
//       fw.ClearDepthStencil();
//       // ... deferred shading passes ...
//       fw.SwapBuffers();
//   }
// =============================================================================

class Framework {
public:
    Framework(const FrameworkDesc& desc = {});
    ~Framework();

    // non-copyable, movable
    Framework(const Framework&)            = delete;
    Framework& operator=(const Framework&) = delete;
    Framework(Framework&&)                 = default;
    Framework& operator=(Framework&&)      = default;

    // ---- Lifecycle ----
    // hInstance: pass GetModuleHandle(nullptr) from WinMain, or omit for default
    bool Initialize(HINSTANCE hInstance = nullptr);
    void Shutdown();

    // ---- Frame helpers (convenience, covers 90%+ of demos) ----

    // Poll input, compute delta, clear backbuffer & depth, set OM targets
    void BeginFrame();

    // Present the swapchain
    void EndFrame();

    // ---- Individual steps (for demos with custom render targets) ----

    // Pump Windows messages, update InputState, compute delta time
    // Returns immediately (non-blocking).
    void PollEvents();

    // Clear the default backbuffer RTV and depth-stencil view, then
    // bind them to the output merger stage.
    void ClearBackBuffer(float r, float g, float b, float a);
    void ClearDepthStencil();

    // Bind only (no clear) — for when you've rendered to custom targets
    // and just need to restore the OM before Present.
    void BindDefaultRenderTargets();

    void SwapBuffers();   // IDXGISwapChain::Present

    bool IsRunning() const;
    void Quit();

    // ---- Accessors ----

    ID3D11Device*           GetDevice()           const;
    ID3D11DeviceContext*    GetContext()          const;
    ID3D11RenderTargetView* GetBackBufferRTV()    const;
    ID3D11DepthStencilView* GetDepthStencilView() const;
    IDXGISwapChain*         GetSwapChain()        const;
    HWND                    GetHwnd()             const;

    int   Width()       const;   // current client-area width
    int   Height()      const;   // current client-area height
    float AspectRatio() const;   // Width() / Height()

    // ---- Timing ----
    float DeltaTime() const;   // seconds since last frame (capped at 0.25s)
    float TotalTime() const;   // seconds since Initialize()

    // ---- Input ----
    const InputState& Input() const;

    // Convenience queries (indexed by Win32 VK_* codes)
    bool KeyDown(int vkCode)    const;   // held this frame
    bool KeyPressed(int vkCode) const;   // just went down this frame
    bool KeyReleased(int vkCode)const;   // just released this frame

    // Mouse convenience (0=left, 1=right, 2=middle)
    bool MouseDown(int button)    const;
    bool MousePressed(int button) const;
    int  MouseX()                 const;
    int  MouseY()                 const;
    int  MouseDeltaX()            const;
    int  MouseDeltaY()            const;
    int  ScrollDelta()            const;

    // ---- Cursor ----
    // Locked: hidden + centered, only MouseDeltaX/Y reported (FPS camera)
    void SetCursorLocked(bool locked);
    bool IsCursorLocked() const;

    // Visible: show/hide without locking (e.g. GUI overlays)
    void SetCursorVisible(bool visible);

    // ---- Callbacks ----
    using ResizeCallback = std::function<void(int newWidth, int newHeight)>;
    void SetResizeCallback(ResizeCallback cb);

    // ---- Utility ----
    // Re-create swapchain buffers (called internally on resize; rarely needed
    // by user code, but exposed for e.g. fullscreen toggle or MSAA change)
    bool ResizeSwapChain(int width, int height);

private:
    struct Impl;
    Impl* m;  // PIMPL — keeps all Win32 + D3D11 internals out of the header
};
