#include <framework/Framework.h>
#include <framework/HrCheck.h>

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <iostream>

using Microsoft::WRL::ComPtr;

// =============================================================================
// LearnD3D11 — 2.1 Hello Triangle
//
// 目标：画出屏幕上第一个三角形。
//
// D3D11 管线的关键步骤：
//   1. 创建顶点数据（CPU 端的三角形顶点位置）
//   2. 编译 HLSL 着色器 → 创建 VertexShader + PixelShader
//   3. 创建 InputLayout（告诉 GPU 顶点数据的内存布局怎样映射到 HLSL 的语义）
//   4. 创建 VertexBuffer（把顶点数据上传到 GPU）
//   5. 渲染循环：设置管线各阶段的状态 → Draw
//
// 与 OpenGL 的关键差异：
//   - D3D11 用 InputLayout 替代了 glVertexAttribPointer 的隐式绑定
//   - 着色器编译是独立步骤（D3DCompile），不是驱动端运行时编译
//   - 每个管线阶段通过 IASet*/VSSet*/PSSet* 显式设置，没有全局状态机
// =============================================================================

// HLSL 着色器源码

const char* vertexShaderSource = R"(
    // POSITION 语义：告诉 InputLayout 这个 float3 来自顶点缓冲区的 POSITION 槽
    // SV_POSITION 语义：告诉光栅化器这是裁剪空间中的顶点位置（必须输出 float4）
    float4 main(float3 pos : POSITION) : SV_POSITION
    {
        return float4(pos, 1.0f);
    }
)";

const char* pixelShaderSource = R"(
    // SV_TARGET 语义：输出到渲染目标（backbuffer）的颜色值
    float4 main() : SV_TARGET
    {
        return float4(1.0f, 0.5f, 0.2f, 1.0f); // 暖橙色
    }
)";

// 编译 HLSL 的辅助函数

ComPtr<ID3DBlob> CompileShader(const char* source, const char* entryPoint,
                               const char* target)
{
    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(
        source, strlen(source),          // 源码及其长度
        nullptr,                         // 可选的文件名（用于错误信息）
        nullptr,                         // 宏定义
        nullptr,                         // include 处理器
        entryPoint,                      // 入口函数名
        target,                          // 着色器模型（如 "vs_5_0", "ps_5_0"）
        0,                               // 编译标志（D3DCOMPILE_DEBUG 等）
        0,                               // 效果编译标志
        shaderBlob.GetAddressOf(),
        errorBlob.GetAddressOf());

    if (FAILED(hr)) {
        if (errorBlob) {
            std::cerr << "Shader compilation error:\n"
                      << static_cast<const char*>(errorBlob->GetBufferPointer())
                      << std::endl;
        } else {
            D3dLogHrFailure(hr, "D3DCompile");
        }
        return nullptr;
    }
    return shaderBlob;
}

int main()
{
    // 1. 初始化 Framework（窗口 + D3D11 设备 + SwapChain）
    Framework fw({.title = L"2.1 Hello Triangle", .width = 800, .height = 600});
    if (!fw.Initialize()) {
        std::cerr << "Failed to initialize Framework." << std::endl;
        return -1;
    }

    ID3D11Device*        device  = fw.GetDevice();
    ID3D11DeviceContext* context = fw.GetContext();

    // 2. 定义顶点数据
    // 注意 D3D11 默认正面 = 顺时针绕序（OpenGL 是逆时针）。
    // 三个顶点按屏幕空间顺时针排列（右下 → 左下 → 顶部），确保不被背面剔除。
    float vertices[] = {
         0.5f, -0.5f, 0.0f,   // 右下
        -0.5f, -0.5f, 0.0f,   // 左下
         0.0f,  0.5f, 0.0f,   // 顶部
    };

    // 3. 编译着色器
    // HLSL 字符串 → D3DCompile → Shader Blob → Create*Shader
    ComPtr<ID3DBlob> vsBlob = CompileShader(vertexShaderSource, "main", "vs_5_0");
    ComPtr<ID3DBlob> psBlob = CompileShader(pixelShaderSource, "main", "ps_5_0");
    if (!vsBlob || !psBlob) {
        std::cerr << "Failed to compile shaders." << std::endl;
        return -1;
    }

    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader>  pixelShader;

    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                            vsBlob->GetBufferSize(),
                                            nullptr,
                                            vertexShader.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateVertexShader"))
        return -1;

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                   psBlob->GetBufferSize(), nullptr,
                                   pixelShader.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreatePixelShader"))
        return -1;

    // 4. 创建 InputLayout
    //
    // 这是 D3D11 与 OpenGL 最大的概念差异之一。
    // OpenGL 用 glVertexAttribPointer 在运行时"描述"当前绑定 VBO 的内存布局；
    // D3D11 要求预先创建一个 InputLayout 对象，它把顶点数据的内存布局与
    // 顶点着色器的输入语义（HLSL 中的 POSITION）正式绑定在一起。
    //
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        {
            "POSITION",                  // 对应 HLSL 中的语义名
            0,                           // 语义索引（POSITION0 / POSITION1）
            DXGI_FORMAT_R32G32B32_FLOAT, // 数据格式：3 个 32 位浮点数
            0,                           // InputSlot: 从哪个顶点缓冲区槽读取
            0,                           // AlignedByteOffset: 该属性在顶点结构中的偏移
            D3D11_INPUT_PER_VERTEX_DATA,
            0                            // InstanceDataStepRate: 0 = 逐顶点
        },
    };

    ComPtr<ID3D11InputLayout> inputLayout;
    hr = device->CreateInputLayout(layoutDesc, ARRAYSIZE(layoutDesc),
                                   vsBlob->GetBufferPointer(), // 对着色器签名验证布局是否匹配
                                   vsBlob->GetBufferSize(),
                                   inputLayout.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateInputLayout"))
        return -1;

    // 5. 创建 VertexBuffer
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage          = D3D11_USAGE_DEFAULT; // GPU 读写，CPU 不访问
    vbd.ByteWidth      = sizeof(vertices);
    vbd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    ComPtr<ID3D11Buffer> vertexBuffer; // 在 OpenGL 中，vertexBuffer 的对应是 VBO
    hr = device->CreateBuffer(&vbd, &initData, vertexBuffer.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateBuffer (vertex)"))
        return -1;

    // 6. 渲染循环
    while (fw.IsRunning()) {
        fw.BeginFrame(); // PollEvents + ClearRenderTargetView + ClearDepthStencilView

        // 设置管线的每个阶段
        // ---- Input Assembler ----
        const UINT stride = 3 * sizeof(float);
        const UINT offset = 0;
        context->IASetInputLayout(inputLayout.Get());
        context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // ---- Vertex Shader ----
        context->VSSetShader(vertexShader.Get(), nullptr, 0);
        // ---- Pixel Shader ----
        context->PSSetShader(pixelShader.Get(), nullptr, 0);
        // ---- Output Merger ----
        // RTV 和 DSV 已在 BeginFrame 中设置

        // 绘制
        context->Draw(3, 0);

        fw.EndFrame(); // Present
    }

    // 7. 清理
    // ComPtr 自动释放，Framework 析构时释放 Device / Context / SwapChain
    fw.Shutdown();
    return 0;
}
