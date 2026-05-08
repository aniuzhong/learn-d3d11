// =============================================================================
// LearnD3D11 — 3.4 Exercise 2: 顶点位置作为颜色输出
//
// 对应 LearnOpenGL 3.6。深入理解光栅化插值行为和 Output Merger 的 clamp。
//
// =============================================================================
// 练习目标
// =============================================================================
//
//   1. 将顶点着色器的输出从"顶点颜色"改为"顶点位置"：
//
//        VSOutput main(VSInput input)
//        {
//            VSOutput output;
//            output.position = float4(input.position, 1.0f);
//            output.ourPosition = input.position;  // 传递位置而非颜色
//            return output;
//        }
//
//   2. 像素着色器接收插值后的位置并作为颜色输出：
//
//        float4 main(PSInput input) : SV_TARGET
//        {
//            return float4(input.ourPosition, 1.0f);
//        }
//
//   3. 观察：三角形的左下角是黑色。为什么？
//      左下顶点的位置是 (-0.5, -0.5, 0.0)，负的 XY 值在写入
//      rendertarget（0~1 范围）时被 Output Merger 的固定功能 clamp 到 0。
//      从三角形的中心线开始，插值变为正值，颜色逐渐显现。
//
// =============================================================================
// D3D11 视角的追问
// =============================================================================
//
//   Q1: 用 TEXCOORD0 语义代替 COLOR0 传递位置，效果一样吗？
//   A1: 一样。非系统语义（COLOR0, TEXCOORD0 等）对光栅化器的插值行为没有区别
//       ——它们都会被透视校正线性插值。只有 SV_POSITION 是系统语义，
//       其插值行为与普通语义不同（不做透视校正）。
//
//   Q2: 负值被 clamp 到 0 是谁做的——Pixel Shader 还是 Output Merger？
//   A2: 是 Output Merger 的固定功能。PS 输出 float4(ourPosition, 1.0f)
//       时值可以是负数，但 rendertarget 格式为 R8G8B8A8_UNORM（0~1 范围），
//       OM 在写入前自动 clamp。本节使用的 backbuffer 格式正是 UNORM。
//
// =============================================================================
// 实现思路
// =============================================================================
//
//   1. 以 3.2 shaders_interpolation 为起点（已有多属性 InputLayout）
//   2. 修改 VS：将 "COLOR0" 语义改为 "TEXCOORD0"，传递 input.position
//      （使用 TEXCOORD0 而不是 COLOR0，顺便回答 Q1）
//   3. 修改 PS：输入也改为 TEXCOORD0，返回 float4(input.ourPosition, 1.0f)
//   4. 顶点位置保持 3.2 的三个三角形顶点即可
//   5. 不需要 cbuffer（本练习焦点是插值和 clamp）
//
// 与 OpenGL 的差异：
//   OpenGL 3.6 直接改 shader 即可。D3D11 版本额外要求：
//     - 语义改名时两边必须同步（TEXCOORD0 → TEXCOORD0）
//     - PSInput struct 必须含 SV_POSITION 占位（3.1 调试中验证的规则）
//     - 尝试去掉 SV_POSITION 占位，看是否会重现签名不兼容错误
// =============================================================================

// TODO: 基于 3.2 shaders_interpolation.cpp 实现上述练习。
// 提示：从 3.2 复制代码，修改 shader 源码字符串即可，无需改 C++ 侧。

#include <cstdio>
int main()
{
    // 本练习尚未实现。请阅读上方注释了解目标和思路，然后编写代码。
    // 建议从 ../3.2.shaders_interpolation/shaders_interpolation.cpp 复制起点代码。
    printf("Exercise 3.4 is not yet implemented.\n");
    printf("See src/1.getting_started/3.4.shaders_exercise2/shaders_exercise2.cpp for instructions.\n");
    return 0;
}
