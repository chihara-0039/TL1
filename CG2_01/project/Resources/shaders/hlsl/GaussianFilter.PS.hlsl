#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

float32_t Gauss(float32_t x, float32_t y, float32_t sigma) {
    static const float32_t kPi = 3.14159265f;
    float32_t exponent = -((x * x) + (y * y)) * rcp(2.0f * sigma * sigma);
    float32_t denominator = 2.0f * kPi * sigma * sigma;
    return exp(exponent) * rcp(denominator);
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(float32_t(width)), rcp(float32_t(height)));

    static const float32_t2 kIndex3x3[3][3] = {
        { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
        { { -1.0f,  0.0f }, { 0.0f,  0.0f }, { 1.0f,  0.0f } },
        { { -1.0f,  1.0f }, { 0.0f,  1.0f }, { 1.0f,  1.0f } },
    };

    float32_t3 color = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t weight = 0.0f;
    for (int32_t y = 0; y < 3; ++y) {
        for (int32_t x = 0; x < 3; ++x) {
            float32_t kernel = Gauss(kIndex3x3[y][x].x, kIndex3x3[y][x].y, 2.0f);
            float32_t2 texcoord = input.texcoord + kIndex3x3[y][x] * uvStepSize;
            color += gTexture.Sample(gSampler, texcoord).rgb * kernel;
            weight += kernel;
        }
    }

    output.color = float32_t4(color * rcp(weight), 1.0f);
    return output;
}
