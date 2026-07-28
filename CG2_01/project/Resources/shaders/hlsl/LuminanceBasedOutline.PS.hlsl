#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSamplerLinear : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

float32_t Luminance(float32_t3 color) {
    return dot(color, float32_t3(0.2125f, 0.7154f, 0.0721f));
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
            float32_t luminance = Luminance(gTexture.Sample(gSamplerLinear, texcoord).rgb);
            difference.x += luminance * kPrewittHorizontalKernel[y][x];
            difference.y += luminance * kPrewittVerticalKernel[y][x];
        }
    }

    float32_t weight = saturate(length(difference) * 6.0f);
    float32_t3 originalColor = gTexture.Sample(gSamplerLinear, input.texcoord).rgb;
    output.color = float32_t4((1.0f - weight) * originalColor, 1.0f);
    return output;
}
