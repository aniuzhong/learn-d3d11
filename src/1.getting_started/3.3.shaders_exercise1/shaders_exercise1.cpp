// =============================================================================
// LearnD3D11 — 3.3 Exercise 1: 修改 VS 逻辑 + 扩展 cbuffer + 16 字节对齐
//
// 本练习合并了 LearnOpenGL 的 3.4（翻转 Y 坐标）和 3.5（添加 uniform 偏移量），体验 cbuffer 的 16 字节对齐陷阱。
//
// =============================================================================
// 练习目标
// =============================================================================
//
// Part A — 修改 VS 逻辑（翻转 Y 坐标）
//
//   1. 以 3.1 shaders_cbuffer 为起点
//   2. 修改顶点着色器，将 Y 坐标取反：
//        output.position = float4(pos.x, -pos.y, pos.z, 1.0f);
//   3. 观察三角形上下颠倒
//
// Part B — 扩展 cbuffer（添加水平偏移量）
//
//   1. 在 HLSL 的 cbuffer SceneData 中添加一个 float xOffset 字段：
//
//        cbuffer SceneData : register(b0)
//        {
//            float4 ourColor;
//            float  xOffset;
//        };
//
//   2. 修改 VS 使用 xOffset：
//        output.position = float4(pos.x + xOffset, -pos.y, pos.z, 1.0f);
//
//   3. 在 C++ 侧给 SceneData struct 添加对应字段：
//
//        struct SceneData
//        {
//            float ourColor[4];  // 16 bytes at offset 0
//            float xOffset;      // 4 bytes at offset 16
//        };
//
//   4. 每帧给 xOffset 赋值（如用键盘 A/D 控制左右移动），通过 UpdateSubresource 上传
//
// Part C — 踩坑：16 字节对齐
//
//   Part B 的 naive 实现很可能出现三角形随机偏移或颜色错乱的 bug。
//
//   原因：HLSL cbuffer 按 16 字节边界排列每个元素：
//
//     HLSL cbuffer 实际布局：           C++ struct（naive）：
//     offset 0:  ourColor  (16 bytes)   float ourColor[4];  16 bytes
//     offset 16: xOffset   (4 bytes)    float xOffset;      4 bytes
//     offset 20: [padding] (12 bytes)   // 缺少 12 bytes padding!
//     总大小:    32 bytes               总大小: 20 bytes ← 不匹配!
//
//   UpdateSubresource 只上传 20 bytes，但 GPU 期望 32 bytes。
//   xOffset 读到脏数据 → 三角形随机偏移。
//
//   解法：
//     struct SceneData
//     {
//         float ourColor[4];  // offset 0,  16 bytes
//         float xOffset;      // offset 16, 4 bytes
//         float _pad[3];      // offset 20, 12 bytes → 填充到 32 bytes
//     };
//
//   或使用 HLSL 的 packoffset 手动控制布局（更精确但更繁琐）。
//
// =============================================================================
// 实现思路
// =============================================================================
//
//   1. 复制 3.1 的完整代码作为起点
//   2. 修改 vertexShaderSource：Y 取反 + 使用 xOffset
//   3. 修改 HLSL cbuffer 和 C++ SceneData：添加 xOffset 和 padding
//   4. 添加键盘输入处理（A 键减小 xOffset，D 键增大 xOffset）
//   5. 先故意不加 padding，观察三角形随机偏移的异常行为
//   6. 添加 padding 修复，对比前后差异
//
// 与 OpenGL 的差异：
//   OpenGL 添加一个 uniform 只需 glUniform1f，驱动自动管理布局。
//   D3D11 的 cbuffer 是"内存块"模型——你必须自己保证 CPU/GPU 两边布局一致。
//   这是 D3D11 新手最容易踩的坑，也是本节的核心教学价值。
// =============================================================================

// TODO: 基于 3.1 shaders_cbuffer.cpp 实现上述练习。
// 提示：可以先复制 3.1 的完整代码过来，再按 Part A → Part B → Part C 逐步修改。

#include <cstdio>
int main()
{
    // 本练习尚未实现。请阅读上方注释了解目标和思路，然后编写代码。
    // 建议从 ../3.1.shaders_cbuffer/shaders_cbuffer.cpp 复制起点代码。
    printf("Exercise 3.3 is not yet implemented.\n");
    printf("See src/1.getting_started/3.3.shaders_exercise1/shaders_exercise1.cpp for instructions.\n");
    return 0;
}
