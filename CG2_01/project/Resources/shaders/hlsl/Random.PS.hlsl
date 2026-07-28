#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSamplerLinear : register(s0);

cbuffer RandomParameter : register(b4) {
    float time;
    int mode;
    float strength;
    float padding;
};

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

float32_t rand2dTo1d(float32_t2 value) {
    float32_t2 smallValue = sin(value);
    float32_t random = dot(smallValue, float32_t2(12.9898f, 78.233f));
    return frac(sin(random) * 43758.5453f);
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    float32_t random = rand2dTo1d(input.texcoord + time);
    if (mode == 0) {
        output.color = float32_t4(random, random, random, 1.0f);
    } else {
        float32_t3 baseColor = gTexture.Sample(gSamplerLinear, input.texcoord).rgb;
        float32_t noise = lerp(1.0f, random, strength);
        output.color = float32_t4(baseColor * noise, 1.0f);
    }

    return output;
}
