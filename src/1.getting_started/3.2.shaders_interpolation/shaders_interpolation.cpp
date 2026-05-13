#include <d3dcompiler.h>
#include <wrl/client.h>

#include <iostream>

#include <utils/Window.h>
#include <utils/Helpers.h>

using Microsoft::WRL::ComPtr;

// =============================================================================
// LearnD3D11 — 3.2 Shaders Interpolation
//
// 目标：理解 D3D11 的多属性顶点布局、HLSL 语义匹配契约、光栅化插值。
//
// 3.1 中三角形颜色来自 Constant Buffer（三个顶点同一颜色）。
// 本节给每个顶点不同的颜色（红/绿/蓝），观察硬件如何在三角形表面
// 自动插值产生渐变效果。
//
// D3D11 独有知识点：
//   1. InputLayout 的多元素定义：每个元素有独立的 SemanticName + AlignedByteOffset
//   2. HLSL 语义作为阶段间"命名契约"——VS 输出的语义名必须与 PS 输入的语义名
//      完全匹配（名字、类型、顺序），不像 GLSL 的 in/out 按声明顺序匹配
//   3. PS 输入必须用 struct 且布局与 VS 输出 struct 完全一致（含 SV_POSITION 占位）
//      ——这是 3.1 调试中验证过的规则
//   4. 光栅化器对非系统语义（COLOR0）进行透视校正插值
//
// 本节关键步骤：
//   1. 顶点数据扩展为 POSITION(float3) + COLOR(float3) 交错排列
//   2. InputLayout 定义两个元素，AlignedByteOffset 正确指向每个属性的起始位置
//   3. VS 接收 POSITION + COLOR，原样传递 COLOR 到输出
//   4. PS 用 struct 接收插值后的 COLOR 并输出
// =============================================================================

// ---- HLSL 着色器源码 ----

const char* vertexShaderSource = R"(
    // VS 输入：来自 Input Assembler，语义名与 InputLayout 的 SemanticName 一致
    // VS 输出：传给 Rasterizer / Pixel Shader，语义名是阶段间匹配的"键"
    struct VSInput
    {
        float3 position : POSITION;
        float3 color    : COLOR0;
    };

    struct VSOutput
    {
        float4 position : SV_POSITION;
        float4 color    : COLOR0;
    };

    VSOutput main(VSInput input)
    {
        VSOutput output;
        output.position = float4(input.position, 1.0f);
        output.color    = float4(input.color, 1.0f);  // 原样传递顶点颜色
        return output;
    }
)";

const char* pixelShaderSource = R"(
    // PS 输入 struct —— 布局与 VSOutput 完全一致（含 SV_POSITION 占位）
    // 光栅化器已将顶点颜色在三角形表面插值，input.color 是插值后的结果
    struct PSInput
    {
        float4 position : SV_POSITION;
        float4 color    : COLOR0;
    };

    float4 main(PSInput input) : SV_TARGET
    {
        return input.color;
    }
)";

int main()
{
    // 1. 创建窗口
    win32::Window window({.title = L"3.2 Shaders Interpolation", .width = 800, .height = 600});
    if (!window.Get()) {
        std::cerr << "Failed to create window." << std::endl;
        return -1;
    }

    // 2. 创建 D3D11 设备 + SwapChain
    ComPtr<ID3D11Device>        d3dDevice;
    ComPtr<ID3D11DeviceContext> d3dContext;
    ComPtr<IDXGISwapChain>      swapChain;
    if (!CreateDeviceAndSwapChain(window.Get(), 800, 600, d3dDevice, d3dContext, swapChain)) {
        std::cerr << "Failed to create D3D11 device." << std::endl;
        return -1;
    }

    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11DepthStencilView> dsv;
    D3D11_VIEWPORT                 vp;
    if (!CreateBackBuffer(d3dDevice.Get(), swapChain.Get(), 800, 600,
                          1, rtv, dsv, vp)) {
        std::cerr << "Failed to create backbuffer." << std::endl;
        return -1;
    }

    window.on_framebuffer_size_ = [&](int w, int h) {
        ResizeBackBuffer(d3dDevice.Get(), swapChain.Get(), d3dContext.Get(),
                         w, h, DXGI_FORMAT_R8G8B8A8_UNORM, 1, rtv, dsv, vp);
    };

    ID3D11Device*        device  = d3dDevice.Get();
    ID3D11DeviceContext* context = d3dContext.Get();

    // 2. 顶点数据：POSITION + COLOR 交错排列
    //
    //    内存布局（每个顶点 24 bytes）：
    //      [0..11]  float3 POSITION  (12 bytes)
    //      [12..23] float3 COLOR     (12 bytes)
    //
    //    三个顶点分别赋予红、绿、蓝，观察光栅化器插值产生的渐变。
    //    D3D11 正面 = 顺时针绕序。
    //
    float vertices[] = {
        // position            // color
         0.5f, -0.5f, 0.0f,    1.0f, 0.0f, 0.0f,  // 右下 — 红
        -0.5f, -0.5f, 0.0f,    0.0f, 1.0f, 0.0f,  // 左下 — 绿
         0.0f,  0.5f, 0.0f,    0.0f, 0.0f, 1.0f,  // 顶部 — 蓝
    };

    // 3. 编译着色器
    ComPtr<ID3DBlob> vsBlob = CompileShader(vertexShaderSource, "main", "vs_5_0");
    ComPtr<ID3DBlob> psBlob = CompileShader(pixelShaderSource, "main", "ps_5_0");
    if (!vsBlob || !psBlob) return -1;

    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader>  pixelShader;

    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                            vsBlob->GetBufferSize(),
                                            nullptr,
                                            vertexShader.GetAddressOf());
    if (!HrSucceeded(hr, "CreateVertexShader")) return -1;

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                   psBlob->GetBufferSize(), nullptr,
                                   pixelShader.GetAddressOf());
    if (!HrSucceeded(hr, "CreatePixelShader")) return -1;

    // 4. 创建 InputLayout —— 两个元素
    //
    //    与 3.1 的关键差异：InputLayout 现在有两个 D3D11_INPUT_ELEMENT_DESC，
    //    分别描述 POSITION 和 COLOR 在顶点缓冲中的位置。
    //
    //    AlignedByteOffset 的含义：
    //      - POSITION: 偏移 0（顶点数据的第一个字段）
    //      - COLOR:    偏移 12（跳过 3 个 float 的 POSITION）
    //
    //    每个元素的 SemanticName 必须与 VS 输入参数的语义名匹配。
    //
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        {
            "POSITION",                   // HLSL VS 中的语义名
            0,                            // 语义索引
            DXGI_FORMAT_R32G32B32_FLOAT,  // 3 个 32 位浮点数
            0,                            // InputSlot
            0,                            // AlignedByteOffset: 0 = 顶点数据起始
            D3D11_INPUT_PER_VERTEX_DATA,
            0                             // 逐顶点
        },
        {
            "COLOR",                      // HLSL VS 中的语义名
            0,                            // 语义索引
            DXGI_FORMAT_R32G32B32_FLOAT,  // 3 个 32 位浮点数（r, g, b）
            0,                            // InputSlot（同一个顶点缓冲）
            12,                           // AlignedByteOffset: 跳过 POSITION(12 bytes)
            D3D11_INPUT_PER_VERTEX_DATA,
            0
        },
    };

    ComPtr<ID3D11InputLayout> inputLayout;
    hr = device->CreateInputLayout(layoutDesc, ARRAYSIZE(layoutDesc),
                                   vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(),
                                   inputLayout.GetAddressOf());
    if (!HrSucceeded(hr, "CreateInputLayout")) return -1;

    // 5. 创建 VertexBuffer
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage          = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth      = sizeof(vertices);
    vbd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vInitData = {};
    vInitData.pSysMem = vertices;

    ComPtr<ID3D11Buffer> vertexBuffer;
    hr = device->CreateBuffer(&vbd, &vInitData, vertexBuffer.GetAddressOf());
    if (!HrSucceeded(hr, "CreateBuffer (vertex)")) return -1;

    // 6. 渲染循环
    while (!window.ShouldClose()) {
        window.PollEvents();
        ClearBackBuffer(d3dContext.Get(), rtv.Get(), dsv.Get(), vp,
                        0.2f, 0.3f, 0.3f, 1.0f);
        ClearDepthStencil(d3dContext.Get(), rtv.Get(), dsv.Get());

        // ---- 设置管线 ----
        // Input Assembler
        // stride = 6 floats × 4 bytes = 24 bytes（每个顶点：3 pos + 3 color）
        UINT stride = 6 * sizeof(float);
        UINT offset = 0;
        context->IASetInputLayout(inputLayout.Get());
        context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Vertex Shader
        context->VSSetShader(vertexShader.Get(), nullptr, 0);

        // Pixel Shader
        context->PSSetShader(pixelShader.Get(), nullptr, 0);

        // 绘制
        context->Draw(3, 0);

        Present(swapChain.Get());
    }

    return 0;
}
