#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t4> gMaskTexture0 : register(t2);
Texture2D<float32_t4> gMaskTexture1 : register(t3);
SamplerState gSamplerLinear : register(s0);

cbuffer DissolveParameter : register(b3) {
    float threshold;
    float edgeWidth;
    int maskIndex;
    float padding;
    float4 edgeColor;
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    float32_t mask = maskIndex == 0
        ? gMaskTexture0.Sample(gSamplerLinear, input.texcoord).r
        : gMaskTexture1.Sample(gSamplerLinear, input.texcoord).r;

    if (mask <= threshold) {
        discard;
    }

    float32_t edge = 1.0f - smoothstep(threshold, threshold + edgeWidth, mask);
    float32_t3 baseColor = gTexture.Sample(gSamplerLinear, input.texcoord).rgb;
    output.color = float32_t4(baseColor + edge * edgeColor.rgb, 1.0f);
    return output;
}
