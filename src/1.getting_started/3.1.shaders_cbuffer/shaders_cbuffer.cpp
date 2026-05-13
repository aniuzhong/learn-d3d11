#include <d3dcompiler.h>
#include <wrl/client.h>

#include <cmath>
#include <iostream>

#include <utils/Window.h>
#include <utils/Helpers.h>

using Microsoft::WRL::ComPtr;

// =============================================================================
// LearnD3D11 — 3.1 Shaders & Constant Buffer
//
// 目标：理解 D3D11 的 CPU→GPU 数据通道。
//
// OpenGL 用 uniform + glUniform4f 逐变量传数据到着色器。
// D3D11 用 Constant Buffer —— 一块 GPU 可见内存，CPU 通过
// UpdateSubresource 写入，GPU 通过 *SetConstantBuffers 绑定到指定 stage。
//
// 这是 D3D11 最核心的数据通道，也是与 OpenGL 最大的心智模型差异：
//   不是"设置一个变量"，
//   而是"更新一块 buffer，再绑到 slot 上"。
//
// 本节关键步骤：
//   1. HLSL 侧声明 cbuffer + register(b0) 绑定到 slot 0
//   2. C++ 侧定义匹配的结构体（布局必须一致）
//   3. CreateBuffer(D3D11_BIND_CONSTANT_BUFFER) 创建 GPU 端缓冲区
//   4. 每帧 UpdateSubresource 写入最新数据
//   5. VSSetConstantBuffers 绑定到 Vertex Shader 的 slot 0
//   6. VS 从 cbuffer 读取颜色，通过 COLOR0 语义传给 PS
//      （必须用 COLOR0 而非 COLOR：当 VS 输出含 SV_POSITION 时，
//       不带索引的 COLOR 会导致 D3D11 阶段间寄存器分配不一致，
//       引发 "Signatures between stages are incompatible" 错误）
//
// 关于 16 字节对齐：
//   本节只用 float4 字段（天然 16 字节对齐），暂时回避布局问题。
//   当 cbuffer 包含 float/float3 等非 float4 字段时，
//   C++ struct 需要手动 padding 匹配 GPU 布局规则。
//   这个陷阱将在 3.3 练习中亲身体验。
// =============================================================================

// ---- HLSL 着色器源码 ----

const char* vertexShaderSource = R"(
    // Constant Buffer: CPU 每帧写入，GPU 着色器只读
    // register(b0) 表示绑定到 constant buffer slot 0
    cbuffer SceneData : register(b0)
    {
        float4 ourColor; // 本节只用 float4（16 bytes），天然对齐
    }

    // VS 输出结构体
    struct VSOutput
    {
        float4 position : SV_POSITION; // 系统语义：裁剪空间位置
        float4 color    : COLOR0;      // 用户语义：传给 Pixel Shader 的颜色
    };

    VSOutput main(float3 pos : POSITION)
    {
        VSOutput output;
        output.position = float4(pos, 1.0f);
        output.color    = ourColor;    // 从 Constant Buffer 读取
        return output;
    }
)";

const char* pixelShaderSource = R"(
    // PS 输入结构体 —— 必须与 VS 输出结构体布局一致（相同的语义、类型、顺序）
    //
    // 关键规则：当 VS 输出含 SV_POSITION 时，PS 输入 struct 也必须声明
    // SV_POSITION 来保证阶段间硬件寄存器分配一致，即使 PS 不使用它。
    // 若 VS 用 struct 而 PS 用裸参数（如 float4 color : COLOR0），
    // 寄存器索引可能偏移，导致 "Signatures between stages are incompatible"。
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

// ---- C++ 侧 Constant Buffer 结构体 ----
// 必须与 HLSL 的 cbuffer SceneData 布局一致！
// HLSL cbuffer 按照 16 字节边界排列每个元素：
//   float4 ourColor → offset 0, 占用 16 bytes
//   总共 16 bytes（恰好是一个 float4）
//
// 如果加一个 float 字段（如 float alpha），HLSL 会把它放在
// offset 16（下一个 16 字节边界），导致 C++ struct 需要 padding。
// 这个陷阱留到 3.3 练习中亲自踩。
struct SceneData
{
    float ourColor[4]; // 对应 HLSL 的 float4 ourColor
};

int main()
{
    // 1. 创建窗口
    win32::Window window({.title = L"3.1 Shaders & Constant Buffer", .width = 800, .height = 600});
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

    // 2. 顶点数据（仅位置，颜色由 cbuffer 提供）
    //    D3D11 正面 = 顺时针绕序
    float vertices[] = {
         0.5f, -0.5f, 0.0f,   // 右下
        -0.5f, -0.5f, 0.0f,   // 左下
         0.0f,  0.5f, 0.0f,   // 顶部
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

    // 4. 创建 InputLayout
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        {
            "POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            0,
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

    // 6. 创建 Constant Buffer
    //
    // 与顶点缓冲的关键区别：
    //   - BindFlags = D3D11_BIND_CONSTANT_BUFFER（GPU 将其视为着色器常量源）
    //   - Usage = D3D11_USAGE_DEFAULT 时，CPU 通过 UpdateSubresource 更新
    //     （如果需要每帧多次更新，应使用 D3D11_USAGE_DYNAMIC + Map/Unmap）
    //   - ByteWidth 必须是 16 的倍数（硬件要求）
    //
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage          = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth      = sizeof(SceneData);
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> constantBuffer;
    hr = device->CreateBuffer(&cbd, nullptr, constantBuffer.GetAddressOf());
    //                          ↑ 初始数据为 nullptr：首帧在循环中通过
    //                            UpdateSubresource 写入
    if (!HrSucceeded(hr, "CreateBuffer (constant)")) return -1;

    // 7. 渲染循环
    while (!window.ShouldClose()) {
        window.PollEvents();
        ClearBackBuffer(d3dContext.Get(), rtv.Get(), dsv.Get(), vp,
                        0.2f, 0.3f, 0.3f, 1.0f);
        ClearDepthStencil(d3dContext.Get(), rtv.Get(), dsv.Get());

        // ---- 每帧更新 Constant Buffer ----
        // OpenGL 的等价操作是 glUniform4f(location, r, g, b, a)。
        // D3D11 需要两步分离：
        //   a) UpdateSubresource — 把 CPU 数据拷到 GPU 缓冲区
        //   b) VSSetConstantBuffers — 把缓冲区绑定到着色器 stage 的 slot
        //
        // 这种分离的代价是多一行代码，但收益是：
        //   - 同一个 cbuffer 可以绑定到多个 stage（VS + PS 共享）
        //   - 可以只更新 cbuffer 的一部分（通过 pDstBox 参数）
        //   - cbuffer 数据可以在多个 Draw 间复用，不用每次重设
        //
        float  timeValue = window.GetTime();
        float  green     = std::sin(timeValue) * 0.5f + 0.5f; // 0~1 之间正弦波动
        SceneData sceneData = {};
        sceneData.ourColor[0] = 0.0f;
        sceneData.ourColor[1] = green;
        sceneData.ourColor[2] = 0.0f;
        sceneData.ourColor[3] = 1.0f;

        context->UpdateSubresource(constantBuffer.Get(), 0, nullptr,
                                   &sceneData, 0, 0);

        // ---- 设置管线 ----
        // Input Assembler
        UINT stride = sizeof(float) * 3;
        UINT offset = 0;
        context->IASetInputLayout(inputLayout.Get());
        context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Vertex Shader + Constant Buffer
        context->VSSetShader(vertexShader.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
        //                              ↑ slot b0，与 HLSL 的 register(b0) 对应

        // Pixel Shader
        context->PSSetShader(pixelShader.Get(), nullptr, 0);

        // 绘制
        context->Draw(3, 0);

        Present(swapChain.Get());
    }

    return 0;
}
