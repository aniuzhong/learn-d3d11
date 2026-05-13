#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <iostream>

// =============================================================================
// HRESULT utilities
// =============================================================================

inline void HrLogError(HRESULT hr, const char* what)
{
    std::cerr << what << " failed (HRESULT 0x" << std::hex
              << static_cast<unsigned int>(static_cast<ULONG>(hr)) << std::dec << ")\n";
}

inline bool HrSucceeded(HRESULT hr, const char* what)
{
    if (SUCCEEDED(hr)) return true;
    HrLogError(hr, what);
    return false;
}

// Macro: stringifies the expression for the error message
#define HR(expr) HrSucceeded((expr), #expr)

// =============================================================================
// D3D11 device + swapchain creation
// =============================================================================

bool CreateDeviceAndSwapChain(
    HWND                                          hwnd,
    int                                           width,
    int                                           height,
    Microsoft::WRL::ComPtr<ID3D11Device>&         outDevice,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>&  outContext,
    Microsoft::WRL::ComPtr<IDXGISwapChain>&       outSwapChain,
    DXGI_FORMAT                                   backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
    int                                           bufferCount      = 2,
    int                                           msaaSamples      = 1,
    bool                                          debugDevice      = false);

// =============================================================================
// Backbuffer RTV + DSV + Viewport creation
// =============================================================================

bool CreateBackBuffer(
    ID3D11Device*                                   device,
    IDXGISwapChain*                                 swapChain,
    int                                             width,
    int                                             height,
    int                                             msaaSamples,
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& outRTV,
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>& outDSV,
    D3D11_VIEWPORT&                                 outViewport);

// =============================================================================
// Resize backbuffer resources
// =============================================================================

bool ResizeBackBuffer(
    ID3D11Device*                                   device,
    IDXGISwapChain*                                 swapChain,
    ID3D11DeviceContext*                            context,
    int                                             width,
    int                                             height,
    DXGI_FORMAT                                     backBufferFormat,
    int                                             msaaSamples,
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv,
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>& dsv,
    D3D11_VIEWPORT&                                 viewport);

// =============================================================================
// Frame-level operations (inline — called per frame)
// =============================================================================

inline void ClearBackBuffer(
    ID3D11DeviceContext*     context,
    ID3D11RenderTargetView*  rtv,
    ID3D11DepthStencilView*  dsv,
    const D3D11_VIEWPORT&    viewport,
    float r, float g, float b, float a)
{
    if (!context || !rtv) return;
    const float color[4] = { r, g, b, a };
    context->ClearRenderTargetView(rtv, color);
    context->OMSetRenderTargets(1, &rtv, dsv);
    context->RSSetViewports(1, &viewport);
}

inline void ClearDepthStencil(
    ID3D11DeviceContext*    context,
    ID3D11RenderTargetView* rtv,
    ID3D11DepthStencilView* dsv)
{
    if (!context || !dsv) return;
    context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &rtv, dsv);
}

inline void BindDefaultRenderTargets(
    ID3D11DeviceContext*    context,
    ID3D11RenderTargetView* rtv,
    ID3D11DepthStencilView* dsv,
    const D3D11_VIEWPORT&   viewport)
{
    if (!context || !rtv) return;
    context->OMSetRenderTargets(1, &rtv, dsv);
    context->RSSetViewports(1, &viewport);
}

inline void Present(IDXGISwapChain* swapChain, bool vSync = true)
{
    if (swapChain) swapChain->Present(vSync ? 1 : 0, 0);
}

// =============================================================================
// Shader compilation
// =============================================================================

Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
    const char* source,
    const char* entryPoint,
    const char* target);
