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

    float32_t3 color = float32_t3(0.0f, 0.0f, 0.0f);
    for (int32_t y = 0; y < 5; ++y) {
        for (int32_t x = 0; x < 5; ++x) {
            float32_t2 offset = float32_t2(float32_t(x - 2), float32_t(y - 2)) * uvStepSize;
            color += gTexture.Sample(gSampler, input.texcoord + offset).rgb * (1.0f / 25.0f);
        }
    }

    output.color = float32_t4(color, 1.0f);
    return output;
}
