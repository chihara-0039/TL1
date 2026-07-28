#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// Constant Buffer for vignette parameters using float4 to guarantee layout matching C++ struct
cbuffer VignetteParameter : register(b0) {
    float4 params; // x: scale, y: exponent, zw: padding
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);

    // 周囲を0に、中心になるほど明るくなるように計算で調整
    float32_t2 correct = input.texcoord * (1.0f - input.texcoord.yx);
    // correctだけで計算すると中心の最大値が0.0625で暗すぎるのでScale (params.x) で調整
    float vignette = correct.x * correct.y * params.x;
    // べき乗 (params.y) で調整して滑らかにする
    vignette = saturate(pow(vignette, params.y));
    // 係数として乗算
    output.color.rgb *= vignette;

    return output;
}
