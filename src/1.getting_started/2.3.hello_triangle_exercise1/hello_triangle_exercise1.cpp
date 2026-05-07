// [ 2.3 练习 ] 用 glDrawArrays 画两个三角形 —— 在同一个 VAO 里增加顶点
//     将 2.1 的顶点从 3 个扩为 6 个、Draw 计数从 3 改为 6，差异仅两行

#include <cstddef>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <iostream>

#include <framework/Framework.h>
#include <framework/HrCheck.h>

using Microsoft::WRL::ComPtr;

const char* vertexShaderSource = R"(
    float4 main(float3 pos : POSITION) : SV_POSITION
    {
        return float4(pos, 1.0f);
    }
)";

const char* pixelShaderSource = R"(
    float4 main() : SV_TARGET
    {
        return float4(1.0f, 0.5f, 0.2f, 1.0f);
    }
)";

ComPtr<ID3DBlob> CompileShader(const char* source, const char* entryPoint,
                               const char* target)
{
    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(
        source,
        strlen(source),
        nullptr,
        nullptr,
        nullptr,
        entryPoint,
        target,
        0,
        0,
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

int main() {
    Framework fw({.title = L"2.3 Hello Triangle Exercise 1", .width = 800, .height = 600});
    if (!fw.Initialize()) {
        std::cerr << "Failed to initialize Framework." << std::endl;
        return -1;
    }

    ID3D11Device*        device  = fw.GetDevice();
    ID3D11DeviceContext* context = fw.GetContext();

    // 定义顶点数据
    //   注意 D3D11 默认正面 = 顺时针绕序（OpenGL 是逆时针）。
    //   两个三角形的三个顶点按屏幕空间顺时针排列（右下 -> 左下 -> 顶部），确保不被背面剔除。
    float vertices[] = {
        // 第一个三角形
        -0.0f, -0.5f, 0.0f,  // right
        -0.9f, -0.5f, 0.0f,  // left
        -0.45f, 0.5f, 0.0f,  // top 
        // 第二个三角形
         0.9f, -0.5f, 0.0f,  // right
         0.0f, -0.5f, 0.0f,  // left
         0.45f, 0.5f, 0.0f   // top 
    }; 

    UINT numVertices = sizeof(vertices) / (3 * sizeof(float)); // 6个顶点，每个顶点 3 个 float 分量

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
    hr = device->CreateInputLayout(layoutDesc,
                                   ARRAYSIZE(layoutDesc),
                                   vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(),
                                   inputLayout.GetAddressOf());

    if (!D3dHrOk(hr, "ID3D11Device::CreateInputLayout"))
        return -1;

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage          = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth      = sizeof(vertices);
    vbd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    ComPtr<ID3D11Buffer> vertexBuffer;
    hr = device->CreateBuffer(&vbd, &initData, vertexBuffer.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateBuffer (vertex)"))
        return -1;

    while (fw.IsRunning()) {
        fw.PollEvents();
        fw.ClearBackBuffer(0.2f, 0.3f, 0.3f, 1.0f);
        fw.ClearDepthStencil();

        const UINT stride = 3 * sizeof(float);
        const UINT offset = 0;
        context->IASetInputLayout(inputLayout.Get());
        context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertexShader.Get(), nullptr, 0);
        context->PSSetShader(pixelShader.Get(), nullptr, 0);
        context->Draw(numVertices, 0);

        fw.SwapBuffers();
    }

    fw.Shutdown();
    return 0;
}