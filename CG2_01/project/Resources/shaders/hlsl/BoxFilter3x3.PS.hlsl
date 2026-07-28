#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(float32_t(width)), rcp(float32_t(height)));

    static const float32_t kKernel3x3[3][3] = {
        { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
        { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
        { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
    };

    static const float32_t2 kIndex3x3[3][3] = {
        { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
        { { -1.0f,  0.0f }, { 0.0f,  0.0f }, { 1.0f,  0.0f } },
        { { -1.0f,  1.0f }, { 0.0f,  1.0f }, { 1.0f,  1.0f } },
    };

    float32_t3 color = float32_t3(0.0f, 0.0f, 0.0f);
    for (int32_t y = 0; y < 3; ++y) {
        for (int32_t x = 0; x < 3; ++x) {
            float32_t2 texcoord = input.texcoord + kIndex3x3[y][x] * uvStepSize;
            color += gTexture.Sample(gSampler, texcoord).rgb * kKernel3x3[y][x];
        }
    }

    output.color = float32_t4(color, 1.0f);
    return output;
}
