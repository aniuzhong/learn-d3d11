// [练习 2.5] 用不同的 PixelShader 给两个三角形上不同颜色
// D3D11 对应知识点: 多 Shader 对象切换 — 创建两个 PixelShader（橙色 + 黄色），
//                 运行时通过 PSSetShader 切换，配合两个 VertexBuffer 分别绘制。
//
// 实现思路:
//   1. 写两个 HLSL pixel shader 入口函数，分别返回 orange 和 yellow 的 float4
//      （也可写在同一 .hlsl 里用不同入口名，或直接硬编码两个不同颜色的 shader 字符串）
//   2. 分别 D3DCompile → CreatePixelShader，得到两个 ID3D11PixelShader 对象
//   3. 创建两个 VertexBuffer（各 3 个顶点，对应左右两个三角形）
//      注意 D3D11 正面 = 顺时针绕序（OpenGL 是逆时针），顶点排列需反转
//      顶点位置参考 OpenGL 2.5：
//        firstTriangle:  left(-0.9,-0.5), right(0.0,-0.5), top(-0.45,0.5)  → CW: right/left/top
//        secondTriangle: left(0.0,-0.5),  right(0.9,-0.5), top(0.45,0.5)   → CW: right/left/top
//   4. 创建一个 InputLayout + 两个 VertexBuffer（顶点格式相同，InputLayout 可跨 VB 复用）
//   5. 渲染循环中: VS / InputLayout / PrimitiveTopology 共用；每三角形只切换 VB + PS
//        context->VSSetShader(vertexShader.Get(), nullptr, 0);
//        context->IASetInputLayout(inputLayout.Get());
//        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//        // 第一个三角形 — 橙色
//        context->IASetVertexBuffers(0, 1, vbo[0].GetAddressOf(), &stride, &offset);
//        context->PSSetShader(pixelShaderOrange.Get(), nullptr, 0);
//        context->Draw(3, 0);
//        // 第二个三角形 — 黄色
//        context->IASetVertexBuffers(0, 1, vbo[1].GetAddressOf(), &stride, &offset);
//        context->PSSetShader(pixelShaderYellow.Get(), nullptr, 0);
//        context->Draw(3, 0);
//   6. 与 OpenGL 的关键差异:
//      - OpenGL 切换整个 shader program（glUseProgram），D3D11 按阶段独立切换（PSSetShader）
//      - D3D11 的 VS 可以共用（两个三角形变换相同），只需切换 PS
//      - 这也展示了 D3D11 管线阶段解耦的优势：不需要为不同 PS 创建两套完整的 program

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <iostream>

#include <utils/Window.h>
#include <utils/Helpers.h>

using Microsoft::WRL::ComPtr;

const char* vertexShaderSource = R"(
    float4 main(float3 pos : POSITION) : SV_POSITION
    {
        return float4(pos, 1.0f);
    }
)";

const char* pixelShader1Source = R"(
    float4 main() : SV_TARGET
    {
        return float4(1.0f, 0.5f, 0.2f, 1.0f);
    }
)";

const char* pixelShader2Source = R"(
    float4 main() : SV_TARGET
    {
        return float4(1.0f, 1.0f, 0.0f, 1.0f);
    }
)";

int main()
{
    win32::Window window({.title = L"2.5 Hello Triangle Exercise3", .width = 800, .height = 600});
    if (!window.Get()) {
        std::cerr << "Failed to create window." << std::endl;
        return -1;
    }

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

    HRESULT hr = S_OK;

    float firstTriangle[] = {
        -0.0f, -0.5f, 0.0f,  // right
        -0.9f, -0.5f, 0.0f,  // left
        -0.45f, 0.5f, 0.0f,  // top
    };
    float secondTriangle[] = {
        0.9f, -0.5f, 0.0f,  // right
        0.0f, -0.5f, 0.0f,  // left
        0.45f, 0.5f, 0.0f   // top
    };

    ComPtr<ID3DBlob> vsBlob = CompileShader(vertexShaderSource, "main", "vs_5_0");
    ComPtr<ID3DBlob> psBlob1 = CompileShader(pixelShader1Source, "main", "ps_5_0");
    ComPtr<ID3DBlob> psBlob2 = CompileShader(pixelShader2Source, "main", "ps_5_0");
    if (!vsBlob || !psBlob1 || !psBlob2)
        return -1;

    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader>  pixelShader1;
    ComPtr<ID3D11PixelShader>  pixelShader2;

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                    vsBlob->GetBufferSize(),
                                    nullptr,
                                    vertexShader.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateVertexShader"))
        return -1;

    hr = device->CreatePixelShader(psBlob1->GetBufferPointer(),
                                   psBlob1->GetBufferSize(), nullptr,
                                   pixelShader1.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreatePixelShader (orange)"))
        return -1;

    hr = device->CreatePixelShader(psBlob2->GetBufferPointer(),
                                   psBlob2->GetBufferSize(), nullptr,
                                   pixelShader2.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreatePixelShader (yellow)"))
        return -1;

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

    // 两个三角形顶点格式完全相同（float3 POSITION），只需一个 InputLayout 即可跨 VB 复用
    ComPtr<ID3D11InputLayout> inputLayout;
    hr = device->CreateInputLayout(layoutDesc, ARRAYSIZE(layoutDesc),
                                   vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(),
                                   inputLayout.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateInputLayout"))
        return -1;
    D3D11_BUFFER_DESC vbd1 = {};
    vbd1.Usage          = D3D11_USAGE_DEFAULT;
    vbd1.ByteWidth      = sizeof(firstTriangle);
    vbd1.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexInitData1 = {};
    vertexInitData1.pSysMem = firstTriangle;
    ComPtr<ID3D11Buffer> vertexBuffer1;
    hr = device->CreateBuffer(&vbd1, &vertexInitData1, vertexBuffer1.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateBuffer (vertex 1)"))
        return -1;

    D3D11_BUFFER_DESC vbd2 = {};
    vbd2.Usage          = D3D11_USAGE_DEFAULT;
    vbd2.ByteWidth      = sizeof(secondTriangle);
    vbd2.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexInitData2 = {};
    vertexInitData2.pSysMem = secondTriangle;
    ComPtr<ID3D11Buffer> vertexBuffer2;
    hr = device->CreateBuffer(&vbd2, &vertexInitData2, vertexBuffer2.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateBuffer (vertex 2)"))
        return -1;

    while (!window.ShouldClose()) {
        window.PollEvents();
        ClearBackBuffer(d3dContext.Get(), rtv.Get(), dsv.Get(), vp,
                        0.2f, 0.3f, 0.3f, 1.0f);
        ClearDepthStencil(d3dContext.Get(), rtv.Get(), dsv.Get());

        UINT stride = sizeof(float) * 3;
        UINT offset = 0;

        // VS、InputLayout、PrimitiveTopology 两个三角形共用，提前设置一次即可
        context->VSSetShader(vertexShader.Get(), nullptr, 0);
        context->IASetInputLayout(inputLayout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // 第一个三角形 — 橙色
        context->IASetVertexBuffers(0, 1, vertexBuffer1.GetAddressOf(), &stride, &offset);
        context->PSSetShader(pixelShader1.Get(), nullptr, 0);
        context->Draw(3, 0);

        // 第二个三角形 — 黄色：只需切换 VB 和 PS
        context->IASetVertexBuffers(0, 1, vertexBuffer2.GetAddressOf(), &stride, &offset);
        context->PSSetShader(pixelShader2.Get(), nullptr, 0);
        context->Draw(3, 0);

        Present(swapChain.Get());
    }

    return 0;
}
