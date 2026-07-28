#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gDepthTexture : register(t1);
SamplerState gSamplerLinear : register(s0);
SamplerState gSamplerPoint : register(s1);

cbuffer OutlineParameter : register(b1) {
    float4x4 projectionInverse;
    float4 outlineParams; // x: depth strength
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

float32_t ConvertViewZ(float32_t depth) {
    float32_t4 ndc = float32_t4(0.0f, 0.0f, depth, 1.0f);
    float32_t4 view = mul(ndc, projectionInverse);
    return view.z * rcp(view.w);
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(float32_t(width)), rcp(float32_t(height)));

    static const float32_t kPrewittHorizontalKernel[3][3] = {
        { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
        { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
        { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    };
    static const float32_t kPrewittVerticalKernel[3][3] = {
        { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
        {  0.0f,        0.0f,        0.0f },
        {  1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f },
    };
    static const float32_t2 kIndex3x3[3][3] = {
        { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
        { { -1.0f,  0.0f }, { 0.0f,  0.0f }, { 1.0f,  0.0f } },
        { { -1.0f,  1.0f }, { 0.0f,  1.0f }, { 1.0f,  1.0f } },
    };

    float32_t2 difference = float32_t2(0.0f, 0.0f);
    for (int32_t y = 0; y < 3; ++y) {
        for (int32_t x = 0; x < 3; ++x) {
            float32_t2 texcoord = input.texcoord + kIndex3x3[y][x] * uvStepSize;
            float32_t viewZ = ConvertViewZ(gDepthTexture.Sample(gSamplerPoint, texcoord));
            difference.x += viewZ * kPrewittHorizontalKernel[y][x];
            difference.y += viewZ * kPrewittVerticalKernel[y][x];
        }
    }

    float32_t weight = saturate(length(difference) * outlineParams.x);
    float32_t3 originalColor = gTexture.Sample(gSamplerLinear, input.texcoord).rgb;
    output.color = float32_t4((1.0f - weight) * originalColor, 1.0f);
    return output;
}
