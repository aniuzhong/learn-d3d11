# 1. Getting Started

LearnOpenGL 第一章从"打开窗口"开始。D3D11 版本做了以下调整：

## 与 LearnOpenGL 的章节差异

| LearnOpenGL | LearnD3D11 | 理由 |
|-------------|-----------|------|
| 1.1 hello_window | **跳过** | OpenGL 需要教 GLFW + GLAD 初始化，D3D11 的 Framework 把这层吸收为一行 `Initialize()`。学习者可阅读 Framework 源码了解 Win32 窗口创建细节。 |
| 1.2 hello_window_clear | **合并到 2.1** | `ClearRenderTargetView` / `ClearDepthStencilView` 的知识在 2.1 中由 `BeginFrame` 说明，单独成节太单薄。 |
| 2.1 hello_triangle | **保留** | 第一个有内容的 D3D11 demo：InputLayout、VertexBuffer、HLSL 编译、管线阶段设置、Draw。 |
| 2.2 hello_triangle_indexed | **保留** | 索引绘制是通用图形学基础，D3D11 的 `IASetIndexBuffer` + `DrawIndexed` 值得独立演示。注意绕序修正。 |
| 2.3 hello_triangle_exercise1 | **合并到 2.1 末尾** | 将 2.1 的顶点从 3 个扩为 6 个、Draw 计数从 3 改为 6，差异仅两行。作为 2.1 末尾的"试试看"提示即可。 |
| 2.4 hello_triangle_exercise2 | **保留，精简** | 两个独立 VBO + 两次 Draw 调用。引入"运行时切换顶点缓冲区"的概念，是真实渲染的基础模式。 |
| 2.5 hello_triangle_exercise3 | **保留，精简** | 两个不同的 PixelShader + 运行时 `PSSetShader` 切换。引入"着色器对象可独立替换"的概念，为后续材质系统铺垫。 |

## D3D11 与 OpenGL 的关键差异点（本章涉及）

| 概念 | OpenGL | D3D11 | 首次出现在 |
|------|--------|-------|-----------|
| 窗口 + 设备创建 | GLFW + GLAD (~30行) | Framework 封装 (~2行) | 2.1 |
| 清除帧缓冲 | `glClearColor` + `glClear` | `ClearRenderTargetView` + `ClearDepthStencilView` | 2.1 (BeginFrame) |
| 渲染循环 | `glfwSwapBuffers` + `glfwPollEvents` | `BeginFrame` + `EndFrame` | 2.1 |
| 着色器语言 | GLSL (驱动端编译) | HLSL (`D3DCompile` 运行时 / fxc 离线) | 2.1 |
| 顶点布局 | `glVertexAttribPointer` | `CreateInputLayout` (预创建对象) | 2.1 |
| 顶点缓冲 | VBO + VAO | `ID3D11Buffer` + `IASetVertexBuffers` | 2.1 |
| 绘制调用 | `glDrawArrays(mode, first, count)` | `Draw(vertexCount, startVertex)` | 2.1 |
| 索引缓冲 | EBO (存储在 VAO) | `IASetIndexBuffer` + `DrawIndexed` | 2.2 |
| 正面绕序 | 默认逆时针 (CCW) | 默认顺时针 (CW) | 2.1, 2.2 |
| 着色器切换 | `glUseProgram(shaderProgram)` | `VSSetShader` / `PSSetShader` | 2.5 |

## D3D11 管线（本章涉及的阶段）

```
  Vertex Buffer --> Input Assembler --> Vertex Shader
       |                   |                  |
  IASetVertexBuffers   IASetInputLayout   VSSetShader
                        IASetPrimitiveTopology
                        
  Index Buffer --> Input Assembler --> ... (2.2 加入)
       |
  IASetIndexBuffer
  DrawIndexed

  Vertex Shader --> Rasterizer --> Pixel Shader --> Output Merger --> BackBuffer
                                        |                  |
                                    PSSetShader     OMSetRenderTargets
                                                    (BeginFrame 自动设置)
```

## 本章学习路径

```
2.1 hello_triangle          第一个三角形：InputLayout + VertexBuffer + HLSL + Draw
2.2 hello_triangle_indexed  索引导出：矩形 = 4 顶点 + 6 索引
2.4 exercise2               两个独立 VBO + 两次 Draw 切换
2.5 exercise3               两个 PixelShader + PSSetShader 运行时切换
3.x shaders                 HLSL 着色器深入、常量缓冲区
4.x textures                纹理映射
5.x transformations         矩阵变换、常量缓冲区传 MVP
6.x coordinate_systems      坐标系统、深度测试
7.x camera                  摄像机类、鼠标键盘控制
```
