#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSamplerLinear : register(s0);

cbuffer RadialBlurParameter : register(b2) {
    float2 center;
    float blurWidth;
    int sampleCount;
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    int clampedSampleCount = max(sampleCount, 1);
    float32_t2 direction = input.texcoord - center;
    float32_t3 color = float32_t3(0.0f, 0.0f, 0.0f);

    for (int32_t sampleIndex = 0; sampleIndex < clampedSampleCount; ++sampleIndex) {
        float32_t ratio = float32_t(sampleIndex) * rcp(float32_t(max(clampedSampleCount - 1, 1)));
        float32_t2 texcoord = input.texcoord + direction * blurWidth * ratio;
        color += gTexture.Sample(gSamplerLinear, texcoord).rgb;
    }

    output.color = float32_t4(color * rcp(float32_t(clampedSampleCount)), 1.0f);
    return output;
}
