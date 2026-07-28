struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    nointerpolation float shape : TEXCOORD1;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // テクスチャサンプリング
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    float2 centeredUv = input.texcoord * 2.0f - 1.0f;
    float softCircleAlpha = saturate((1.0f - length(centeredUv)) * 3.0f);
    float lineSideAlpha = saturate((1.0f - abs(centeredUv.x)) * 4.0f);
    float lineEndAlpha = saturate((1.0f - abs(centeredUv.y)) * 5.0f);
    float solidLineAlpha = lineSideAlpha * lineEndAlpha;
    float cloudNoise =
        sin(input.texcoord.x * 41.0f + input.texcoord.y * 17.0f) * 0.08f +
        sin(input.texcoord.x * 19.0f - input.texcoord.y * 37.0f) * 0.06f;
    float cloudCore = saturate((1.0f - length(centeredUv * float2(0.78f, 1.22f))) * 1.55f);
    float cloudLobeA = saturate((0.74f - length(centeredUv - float2(-0.38f, -0.06f))) * 1.72f);
    float cloudLobeB = saturate((0.70f - length(centeredUv - float2(0.34f, 0.10f))) * 1.66f);
    float cloudLobeC = saturate((0.58f - length(centeredUv - float2(0.03f, -0.34f))) * 1.52f);
    float cloudAlpha = saturate(max(max(cloudCore, cloudLobeA), max(cloudLobeB, cloudLobeC)) + cloudNoise);
    cloudAlpha = smoothstep(0.08f, 0.78f, cloudAlpha) * 0.82f;
    float shapeAlpha = input.shape > 1.5f
        ? cloudAlpha
        : input.shape > 0.5f ? solidLineAlpha : softCircleAlpha;

    // テクスチャの色 * パーティクルの色
    output.color = textureColor * input.color;
    output.color.a *= shapeAlpha;
    
    // アルファテスト (完全に透明な部分は描画しない)
    if (output.color.a <= 0.001f)
    {
        discard;
    }
    
    return output;
}
