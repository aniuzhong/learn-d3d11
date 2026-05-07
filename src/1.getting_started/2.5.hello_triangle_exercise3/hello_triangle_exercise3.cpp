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
//   4. 创建两个 VAO（各绑定对应的 VBO），或直接创建两个 InputLayout（如果顶点格式相同可共用）
//      D3D11 没有 VAO，但可以分别创建两个 InputLayout + 两个 VertexBuffer 来对应这个概念
//   5. 渲染循环中:
//        context->PSSetShader(pixelShaderOrange.Get(), nullptr, 0);
//        context->IASetVertexBuffers(0, 1, vbo[0].GetAddressOf(), &stride, &offset);
//        context->Draw(3, 0);
//        context->PSSetShader(pixelShaderYellow.Get(), nullptr, 0);
//        context->IASetVertexBuffers(0, 1, vbo[1].GetAddressOf(), &stride, &offset);
//        context->Draw(3, 0);
//   6. 与 OpenGL 的关键差异:
//      - OpenGL 切换整个 shader program（glUseProgram），D3D11 按阶段独立切换（PSSetShader）
//      - D3D11 的 VS 可以共用（两个三角形变换相同），只需切换 PS
//      - 这也展示了 D3D11 管线阶段解耦的优势：不需要为不同 PS 创建两套完整的 program

int main() {
    return 0;
}
