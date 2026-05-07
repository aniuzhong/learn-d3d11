#include <cstdint>
#include <framework/Framework.h>
#include <framework/HrCheck.h>

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <iostream>

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
        source, strlen(source),
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

int main()
{
    Framework fw({.title = L"2.2 Hello Triangle Indexed", .width = 800, .height = 600});
    if (!fw.Initialize()) {
        std::cerr << "Failed to initialize Framework." << std::endl;
        return -1;
    }

    ID3D11Device*        device  = fw.GetDevice();
    ID3D11DeviceContext* context = fw.GetContext();

    // 4 个顶点（矩形四角），供 2 个三角形通过索引共享顶点
    // 注意 D3D11 正面 = 顺时针绕序
    float vertices[] = {
         0.5f,  0.5f, 0.0f,   // 右上
         0.5f, -0.5f, 0.0f,   // 右下
        -0.5f, -0.5f, 0.0f,   // 左下
        -0.5f,  0.5f, 0.0f,   // 左上
    };

    ComPtr<ID3DBlob> vsBlob = CompileShader(vertexShaderSource, "main", "vs_5_0");
    ComPtr<ID3DBlob> psBlob = CompileShader(pixelShaderSource, "main", "ps_5_0");
    if (!vsBlob || !psBlob)
        return -1;

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
    hr = device->CreateInputLayout(layoutDesc, ARRAYSIZE(layoutDesc),
                                   vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(),
                                   inputLayout.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateInputLayout"))
        return -1;

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage          = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth      = sizeof(vertices);
    vbd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexInitData = {};
    vertexInitData.pSysMem = vertices;

    ComPtr<ID3D11Buffer> vertexBuffer;
    hr = device->CreateBuffer(&vbd, &vertexInitData, vertexBuffer.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateBuffer (vertex)"))
        return -1;

    // 1. 准备索引数据 (CPU 端)
    // 两个三角形拼成矩形，共享顶点 1 和 3
    // 三角形1: 右上->右下->左下 (0,1,2)，
    // 三角形2: 右上->左下->左上 (0,2,3)
    uint32_t indices[] = {
        0, 1, 2,   // 第一个三角形（下半部分，顺时针绕序）
        0, 2, 3    // 第二个三角形（上半部分，顺时针绕序）
    };

    // 2. 创建索引缓冲区 (GPU 端)
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage          = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth      = sizeof(indices);
    ibd.BindFlags      = D3D11_BIND_INDEX_BUFFER; // 关键：告知 GPU 这是索引

    D3D11_SUBRESOURCE_DATA indexInitData = {};
    indexInitData.pSysMem = indices;

    ComPtr<ID3D11Buffer> indexBuffer;
    hr = device->CreateBuffer(&ibd, &indexInitData, indexBuffer.GetAddressOf());
    if (!D3dHrOk(hr, "ID3D11Device::CreateBuffer (index)"))
        return -1;

    while (fw.IsRunning()) {
        fw.BeginFrame();
        const UINT stride = 3 * sizeof(float);
        const UINT offset = 0;
        context->IASetInputLayout(inputLayout.Get());
        context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);

        // 3. 绑定索引缓冲区
        // 注意：DXGI_FORMAT_R32_UINT 必须与 CPU 端定义的 uint32_t 对应。
        context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertexShader.Get(), nullptr, 0);
        context->PSSetShader(pixelShader.Get(), nullptr, 0);

        // 4. 使用索引绘制：6 个索引 = 2 个三角形 = 1 个矩形
        // 与 LearnOpenGL 的 glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0) 等效
        context->DrawIndexed(6, 0, 0);
        fw.EndFrame();
    }

    fw.Shutdown();
    return 0;
}
