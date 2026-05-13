#include <utils/Helpers.h>

#include <algorithm>
#include <iostream>

using Microsoft::WRL::ComPtr;

// =============================================================================
// D3D11 device + swapchain creation
// =============================================================================

bool CreateDeviceAndSwapChain(
    HWND                          hwnd,
    int                           width,
    int                           height,
    ComPtr<ID3D11Device>&         outDevice,
    ComPtr<ID3D11DeviceContext>&  outContext,
    ComPtr<IDXGISwapChain>&       outSwapChain,
    DXGI_FORMAT                   backBufferFormat,
    int                           bufferCount,
    int                           msaaSamples,
    bool                          debugDevice)
{
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
    };
    D3D_FEATURE_LEVEL chosenLevel;

    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (debugDevice) createFlags |= D3D11_CREATE_DEVICE_DEBUG;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferDesc.Width                   = width;
    sd.BufferDesc.Height                  = height;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format                  = backBufferFormat;
    sd.SampleDesc.Count                   = std::max(1, msaaSamples);
    sd.SampleDesc.Quality                 = msaaSamples > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount                        = bufferCount;
    sd.OutputWindow                       = hwnd;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags                              = 0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &sd,
        outSwapChain.GetAddressOf(),
        outDevice.GetAddressOf(),
        &chosenLevel,
        outContext.GetAddressOf());

    if (FAILED(hr) && debugDevice) {
        createFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
            featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
            &sd, outSwapChain.GetAddressOf(), outDevice.GetAddressOf(),
            &chosenLevel, outContext.GetAddressOf());
    }
    return HrSucceeded(hr, "D3D11CreateDeviceAndSwapChain");
}

// =============================================================================
// Backbuffer RTV + DSV + Viewport creation
// =============================================================================

bool CreateBackBuffer(
    ID3D11Device*                   device,
    IDXGISwapChain*                 swapChain,
    int                             width,
    int                             height,
    int                             msaaSamples,
    ComPtr<ID3D11RenderTargetView>& outRTV,
    ComPtr<ID3D11DepthStencilView>& outDSV,
    D3D11_VIEWPORT&                 outViewport)
{
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (!HrSucceeded(hr, "IDXGISwapChain::GetBuffer(0)")) return false;

    hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, outRTV.GetAddressOf());
    if (!HrSucceeded(hr, "ID3D11Device::CreateRenderTargetView")) return false;

    D3D11_TEXTURE2D_DESC dsd = {};
    dsd.Width              = width;
    dsd.Height             = height;
    dsd.MipLevels          = 1;
    dsd.ArraySize          = 1;
    dsd.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsd.SampleDesc.Count   = std::max(1, msaaSamples);
    dsd.SampleDesc.Quality = msaaSamples > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
    dsd.Usage              = D3D11_USAGE_DEFAULT;
    dsd.BindFlags          = D3D11_BIND_DEPTH_STENCIL;

    ComPtr<ID3D11Texture2D> depthTexture;
    hr = device->CreateTexture2D(&dsd, nullptr, depthTexture.GetAddressOf());
    if (!HrSucceeded(hr, "ID3D11Device::CreateTexture2D (depth stencil)")) return false;

    hr = device->CreateDepthStencilView(depthTexture.Get(), nullptr, outDSV.GetAddressOf());
    if (!HrSucceeded(hr, "ID3D11Device::CreateDepthStencilView")) return false;

    outViewport.TopLeftX = 0.0f;
    outViewport.TopLeftY = 0.0f;
    outViewport.Width    = static_cast<float>(width);
    outViewport.Height   = static_cast<float>(height);
    outViewport.MinDepth = 0.0f;
    outViewport.MaxDepth = 1.0f;

    return true;
}

// =============================================================================
// Resize backbuffer resources
// =============================================================================

bool ResizeBackBuffer(
    ID3D11Device*                   device,
    IDXGISwapChain*                 swapChain,
    ID3D11DeviceContext*            context,
    int                             width,
    int                             height,
    DXGI_FORMAT                     backBufferFormat,
    int                             msaaSamples,
    ComPtr<ID3D11RenderTargetView>& rtv,
    ComPtr<ID3D11DepthStencilView>& dsv,
    D3D11_VIEWPORT&                 viewport)
{
    if (width <= 0 || height <= 0) return false;

    context->OMSetRenderTargets(0, nullptr, nullptr);
    rtv.Reset();
    dsv.Reset();

    HRESULT hr = swapChain->ResizeBuffers(0, width, height, backBufferFormat, 0);
    if (!HrSucceeded(hr, "IDXGISwapChain::ResizeBuffers")) return false;

    return CreateBackBuffer(device, swapChain, width, height,
                            msaaSamples, rtv, dsv, viewport);
}

// =============================================================================
// Shader compilation
// =============================================================================

ComPtr<ID3DBlob> CompileShader(
    const char* source,
    const char* entryPoint,
    const char* target)
{
    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(
        source, strlen(source),
        nullptr, nullptr, nullptr,
        entryPoint, target,
        0, 0,
        shaderBlob.GetAddressOf(),
        errorBlob.GetAddressOf());

    if (FAILED(hr)) {
        if (errorBlob) {
            std::cerr << "Shader compilation error:\n"
                      << static_cast<const char*>(errorBlob->GetBufferPointer())
                      << std::endl;
        } else {
            HrLogError(hr, "D3DCompile");
        }
        return nullptr;
    }
    return shaderBlob;
}
