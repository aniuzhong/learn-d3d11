# 1. Getting Started

LearnOpenGL 第一章从"打开窗口"开始。D3D11 版本做了以下调整：

## 与 LearnOpenGL 的章节差异

| LearnOpenGL | LearnD3D11 | 理由 |
|-------------|-----------|------|
| 1.1 hello_window | **跳过** | OpenGL 需要教 GLFW + GLAD 初始化，D3D11 的 Framework 把这层吸收为一行 `Initialize()`。学习者可阅读 Framework 源码了解 Win32 窗口创建细节。 |
| 1.2 hello_window_clear | **合并到 2.1** | `ClearRenderTargetView` / `ClearDepthStencilView` 的知识在 2.1 中由 `BeginFrame` 说明，单独成节太单薄。 |
| 2.1 hello_triangle | **保留** | 第一个有内容的 D3D11 demo：InputLayout、VertexBuffer、HLSL 编译、管线阶段设置、Draw。 |
| 2.2 hello_triangle_indexed | **保留** | 索引绘制是通用图形学基础，D3D11 的 `IASetIndexBuffer` + `DrawIndexed` 值得独立演示。注意绕序修正。 |
| 2.3 hello_triangle_exercise1 | **保留** | 在同一个 VertexBuffer 里放入 6 个顶点（两个三角形并排），`Draw(6,0)` 一次画出。实现正确：顶点绕序已按 D3D11 顺时针正面翻转，位置坐标与 OpenGL 版一致。 |
| 2.4 hello_triangle_exercise2 | **删除** | OpenGL 2.4 的教学目的是"使用两个独立 VAO 切换图元"。D3D11 没有 VAO 概念——`InputLayout` 只描述顶点格式不绑定缓冲区，"切换 VAO"在 D3D11 里直接退化为 `IASetVertexBuffers` 换 buffer 后 draw，没有新 API 或新概念可教学。保留为独立练习价值过低。 |
| 2.5 hello_triangle_exercise3 | **保留** | 两个不同的 PixelShader（橙色+黄色）+ 运行时 `PSSetShader` 切换。引入"着色器对象可独立替换"的概念，为后续材质系统铺垫。 |
| 3.1 shaders_uniform | **重构** → 3.1 shaders_cbuffer | OpenGL 用 `uniform` + `glUniform4f` 逐变量传递 CPU 数据；D3D11 用 Constant Buffer（整块 GPU 可见内存，`cbuffer` + `CreateBuffer` + `UpdateSubresource` + `*SetConstantBuffers`）。这是 D3D11 最核心的数据通道，不仅是 API 替换而是——从"设一个变量"变成"更新一块 buffer 再绑定到 slot"。3.1 刻意只用 `float4` 字段回避 16 字节对齐问题，先关注 buffer 创建→更新→绑定的核心流程。动画颜色效果与 OpenGL 3.1 一致。 |
| 3.2 shaders_interpolation | **保留** → 3.2 shaders_interpolation | 光栅化插值是图形学通用概念。D3D11 版本在两个维度上深化：(1) 多属性 InputLayout（POSITION + COLOR）和交错顶点缓冲；(2) **HLSL 语义作为阶段间命名契约**——VS 输出与 PS 输入通过语义名匹配（而非 GLSL 的声明顺序），名字不同则数据错乱。 |
| 3.3 shaders_class | **跳过** | OpenGL 的 shader 编译+链接样板码多达 ~40 行，封装成类有明显收益。D3D11 仅需 `D3DCompile` + `CreateVertexShader` + `CreatePixelShader` 三步，且项目中已有的 `CompileShader()` 辅助函数已足够。强行封装反而增加理解负担。 |
| 3.4 shaders_exercise1 | **合并到 3.3** | 原 3.4（改 VS 翻转 Y）和 3.5（给 cbuffer 加 xOffset 字段）各自太单薄，合并为一个练习。合并后引出 D3D11 陷阱——给 cbuffer 加非 `float4` 字段后 C++ struct 需要 padding 匹配 16 字节对齐，否则 UpdateSubresource 数据错位导致三角形随机偏移。 |
| 3.5 shaders_exercise2 | **合并到 3.3** | 见上。 |
| 3.6 shaders_exercise3 | **保留** → 3.4 | 练习输出顶点位置作为颜色，深入理解光栅化插值行为。D3D11 视角的追问：(1) 用 `TEXCOORD0` 语义传递位置（证明非 COLOR 语义也能做数据桥梁）；(2) 负值被 Output Merger 固定功能 clamp 到 0，不是 shader 行为。 |

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
| 着色器切换 | `glUseProgram(shaderProgram)` | `VSSetShader` / `PSSetShader` (分阶段独立切换) | 2.5 |
| CPU→GPU 数据传递 | `uniform` + `glUniform*` (逐变量查询 location) | Constant Buffer (`cbuffer` + `CreateBuffer` + `UpdateSubresource` + `*SetConstantBuffers`，按 slot 绑定) | 3.1 |
| 着色器间数据传递 | `in` / `out` 关键字（按声明顺序匹配） | 语义 (Semantics: 按名字匹配；必须用 `COLOR0`/`TEXCOORD0` 带索引形式。PS 输入 struct 布局必须与 VS 输出 struct 完全一致——含 `SV_POSITION` 占位，即使 PS 不用) | 3.1, 3.2 |
| 阶段间签名验证 | OpenGL linker 自动处理 | D3D11 无 linker，Draw 时校验 VS 输出与 PS 输入签名是否兼容；不兼容则报 `DEVICE_SHADER_LINKAGE_REGISTERINDEX` | 3.1 |
| HLSL cbuffer 内存布局 | N/A (GLSL 自动布局) | 16 字节对齐规则；C++ struct 必须与 HLSL cbuffer 布局一致 | 3.1 (提及), 3.3 (实战) |

## D3D11 管线（本章涉及的阶段）

```
  Constant Buffer --> Vertex Shader (3.1 加入，3.3 扩展)
       |                  |
  VSSetConstantBuffers  VSSetShader

  Vertex Buffer --> Input Assembler --> Vertex Shader
       |                   |              
  IASetVertexBuffers   IASetInputLayout   
                       IASetPrimitiveTopology
                       
  Index Buffer --> Input Assembler --> ... (2.2 加入)
       |
  IASetIndexBuffer
  DrawIndexed

  Vertex Shader --> Rasterizer --> Pixel Shader --> Output Merger --> BackBuffer
       |                               |                  |
   语义传递 (3.2)                  PSSetShader     OMSetRenderTargets
  (名字匹配契约)           PSSetConstantBuffers (3.1)  (BeginFrame 自动设置)
  
  Constant Buffer --> Pixel Shader (3.1 加入)
       |
  PSSetConstantBuffers
```

## 本章学习路径

```
2.1 hello_triangle            第一个三角形：InputLayout + VertexBuffer + HLSL + Draw
2.2 hello_triangle_indexed    索引导出：矩形 = 4 顶点 + 6 索引
2.3 hello_triangle_exercise1  同一 VBO 扩至 6 顶点，一次 Draw 画两个三角形
2.5 hello_triangle_exercise3  两个 PixelShader 切换，两个三角形上不同颜色
3.1 shaders_cbuffer           Constant Buffer：CPU→GPU 数据通道（cbuffer + UpdateSubresource），16 字节对齐初识
3.2 shaders_interpolation     多属性顶点布局 + HLSL 语义命名契约 + 光栅化插值
3.3 shaders_exercise1         修改 VS + 扩展 cbuffer + 16 字节对齐练习（合并原 3.4+3.5）
3.4 shaders_exercise2         输出顶点位置为颜色，深入理解插值与 Output Merger clamp
4.x textures                  纹理映射
5.x transformations           矩阵变换、常量缓冲区传 MVP
6.x coordinate_systems        坐标系统、深度测试
7.x camera                    摄像机类、鼠标键盘控制
```
