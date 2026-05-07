# Framework 设计理由

- [Framework 设计理由](#framework-设计理由)
  - [定位](#定位)
  - [为什么不用 GLFW 做 D3D11](#为什么不用-glfw-做-d3d11)
  - [设计思路](#设计思路)
    - [1. 窗口创建与 D3D11 设备创建合并在一个类](#1-窗口创建与-d3d11-设备创建合并在一个类)
    - [2. PIMPL 模式](#2-pimpl-模式)
    - [3. 两层 API 设计](#3-两层-api-设计)
    - [4. 输入使用 Win32 VK\_\* 虚拟键码](#4-输入使用-win32-vk_-虚拟键码)
    - [5. Resize 自动处理](#5-resize-自动处理)
    - [6. 默认深度模板缓冲](#6-默认深度模板缓冲)
  - [配置结构体](#配置结构体)
  - [各章节适配矩阵](#各章节适配矩阵)

## 定位

Framework 在 LearnD3D11 中的角色等同于 GLFW 在 LearnOpenGL 中的角色——提供窗口创建、输入轮询、渲染循环基础设施，更好地把精力集中在 D3D11 图形 API 本身。

但不同于 GLFW 是一套跨平台的 C 库，Framework 是专门为 Windows + D3D11 场景编写的轻量 C++ 封装。它不追求通用性，只服务于学习。

## 为什么不用 GLFW 做 D3D11

GLFW 的核心设计围绕 OpenGL Context 展开。在 D3D11 场景下引入 GLFW 会带来几个问题：

1. **GLFW 仍然要求创建 OpenGL Context**（或通过 `GLFW_NO_API` hint 跳过），但 D3D11 使用 `D3D11CreateDeviceAndSwapChain` 独立创建设备和交换链，与 GL Context 完全无关。
2. **仍然需要提取 HWND**：D3D11 创建 SwapChain 必须传入原生 `HWND`，GLFW 的 `glfwGetWin32Window()` 只是一个 getter，不省代码量。
3. **额外的 DLL 依赖**：纯 Win32 方案零外部依赖。
4. **消息循环控制权受限**：GLFW 封装了 `GetMessage`/`PeekMessage`，在需要精细控制消息泵的场景（组合 IMGUI、处理 WM_SIZE 重建 SwapChain）反而碍事。

## 设计思路

### 1. 窗口创建与 D3D11 设备创建合并在一个类

OpenGL 模型（GLFW 负责窗口 + GL Context，GLAD 负责加载函数指针，应用自己组合）的分离是有历史原因的。D3D11 没有函数指针加载这层概念（COM 接口天然就是动态加载的），而 SwapChain 创建时需要 HWND 和 Device 同时出现，把窗口和设备分离反而增加复杂度。

合并的好处：
- 一个对象同时持有 Window + Device + Context + SwapChain + RTV + DSV
- 应用代码只需 `Framework fw({...}); fw.Initialize();`
- 销毁时自动释放所有 COM 资源（PIMPL 析构函数 + ComPtr）

### 2. PIMPL 模式

头文件只暴露接口和配置结构体，所有 Win32 细节（WNDCLASSEX、窗口过程、消息循环）和 D3D11 COM 对象（Device、Context、SwapChain、RTV、DSV、DepthStencil texture）全部隐藏在 .cpp 的 `Impl` 结构体中。

对学习者来说，头文件即文档——不需要理解 Win32 窗口过程就能开始写 D3D11 代码。（后续的深入学习是不可避免的）

### 3. 两层 API 设计

```
BeginFrame / EndFrame                                                                      <- 简洁模式，95% 的 demo 只需要这一对
        |
        ▼
PollEvents / ClearBackBuffer / ClearDepthStencil / BindDefaultRenderTargets / SwapBuffers  <- 细粒度模式，高级 demo 手动编排多 pass 渲染
```

**简洁模式**适合简单 demo：

```cpp
while (fw.IsRunning()) {
    fw.BeginFrame();   // PollEvents + 清除 backbuffer + 清除深度模板 + 绑定 OM
    // ... 渲染 ...
    fw.EndFrame();     // Present
}
```

**细粒度模式**适合需要渲染到自定义 RenderTarget 的高级 demo（阴影映射、延迟渲染、后处理）：

```cpp
while (fw.IsRunning()) {
    fw.PollEvents();
    // 渲染到 shadow map ...
    // 渲染到 G-Buffer ...
    fw.ClearBackBuffer(0.1f, 0.1f, 0.1f, 1.0f);  // 恢复默认渲染目标
    fw.ClearDepthStencil();
    // 光照 pass ...
    fw.SwapBuffers();
}
```

### 4. 输入使用 Win32 VK_* 虚拟键码

不做自定义键码枚举——直接用 Windows 原生的 `VK_ESCAPE`、`VK_W`、`VK_SPACE` 等。目标用户是 Windows 程序员，VK_* 键码是共识。

提供三态查询：

| 方法 | 含义 | 典型用途 |
|------|------|----------|
| `KeyDown(VK_W)` | 当前帧按住 | 持续移动摄像机 |
| `KeyPressed(VK_SPACE)` | 本帧刚按下 | 切换状态（线框/实体） |
| `KeyReleased(VK_ESCAPE)` | 本帧刚释放 | 不太常用 |

### 5. Resize 自动处理

D3D11 窗口 resize 的完整流程：释放旧的 RTV/DSV/DepthTexture → `SwapChain::ResizeBuffers()` → 重新获取 backbuffer → 创建新的 RTV + DepthStencil + DSV。这个过程容易出错，Framework 内部自动完成。

App 通过 `SetResizeCallback` 获知 resize 事件以重建自己的视口或自定义渲染目标。

### 6. 默认深度模板缓冲

90% 以上的 demo 需要深度测试。Framework 在 SwapChain 创建时同时创建配套的 DepthStencil Texture + View（格式 `D24_UNORM_S8_UINT`），BeginFrame 自动清除深度和模板。少数不需要深度测试的 demo 不设置 `DepthStencilState` 即可——硬件不会写入深度。

这省去了每个 demo 手动创建 DepthStencil 的 ~20 行模板代码。

## 配置结构体

| 字段 | 默认值 | 用途覆盖 |
|------|--------|----------|
| `title` | `L"LearnD3D11"` | 全部 |
| `width` / `height` | 800 / 600 | 全部 |
| `resizable` | `true` | Ch4 stencil_testing 需固定窗口 |
| `msaaSamples` | 1 (off) | Ch4.11 MSAA 需 4x |
| `backBufferFormat` | `R8G8B8A8_UNORM` | Ch5.6 HDR 需 `R16G16B16A16_FLOAT` |
| `debugDevice` | `false` | Ch7.1 debugging 需开启 |
| `vSync` | `true` | 全部 |

## 各章节适配矩阵

| 章节 | 特殊需求 | Framework 如何支持 |
|------|----------|-------------------|
| Ch1 Getting Started | 摄像机光标锁定 | `SetCursorLocked(true)` |
| Ch2 Lighting | — | 无特殊需求 |
| Ch3 Model Loading | HWND 用于文件对话框 | `GetHwnd()` |
| Ch4 Advanced | 模板缓冲、MSAA、渲染到纹理 | DSV 格式含 stencil、`msaaSamples` 配置、手动管理 RTV |
| Ch5 Advanced Lighting | HDR backbuffer、阴影映射 | `backBufferFormat` 配置、手建深度 RTV |
| Ch6 PBR | HDR cubemap | 应用级 |
| Ch7 In Practice | Debug device、固定窗口 | `debugDevice`、`resizable=false` |
| Ch8 Guest | Compute Shader、曲面细分 | D3D11 FL 11.0 原生支持 |
