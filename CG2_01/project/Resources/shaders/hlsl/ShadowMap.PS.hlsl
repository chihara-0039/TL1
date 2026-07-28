float4 main() : SV_TARGET
{
}

// 1. 変換行列を受け取る定数バッファ
struct TransformationMatrix
{
    float32_t4x4 WVP; // World-View-Projection 行列
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// 2. 入力レイアウト（座標だけでOK）
struct VertexShaderInput
{
    float32_t4 position : POSITION;
};

// 3. 出力（システム用の座標のみ）
struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // ライト視点の行列で座標変換するだけ！
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    return output;
}