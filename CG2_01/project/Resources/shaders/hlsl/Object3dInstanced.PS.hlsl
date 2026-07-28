struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 lightSpacePosition : POSITION0;
    float3 worldPosition : POSITION1;
    
    float4 color : COLOR0;
    float shininess : SHININESS0;
    float metallic : METALLIC0;
    float emissive : EMISSIVE0;
};

static const uint MAX_POINT_LIGHTS = 8;

struct PointLight
{
    float3 position;
    float intensity;
    float4 color;
    float radius;
    float3 padding;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
    float3 cameraPosition;
    float paddingLight;
    PointLight pointLights[MAX_POINT_LIGHTS];
    uint pointLightCount;
    float3 pointLightPadding;
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
Texture2D<float> gShadowMap : register(t1);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    float4 matColor = input.color;
    float matShininess = input.shininess;
    float matMetallic = input.metallic;
    float matEmissive = input.emissive;
    
    float3 lightPos = input.lightSpacePosition.xyz / input.lightSpacePosition.w;
    float2 shadowUV = lightPos.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    float currentDepth = lightPos.z;
    
    float shadowFactor = 1.0f;
    
    if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f && shadowUV.y >= 0.0f && shadowUV.y <= 1.0f)
    {
        float mapDepth = gShadowMap.Sample(gSampler, shadowUV);
        float depthDiff = currentDepth - mapDepth;
        if (depthDiff > 0.0005f)
        {
            float fade = saturate(1.0f - (depthDiff / 0.05f));
            shadowFactor = lerp(1.0f, 0.6f, fade);
        }
    }

    float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
    float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
    float ambient = 0.35f;
    
    // 1. Diffuse
    float3 diffuseColor = (cos * shadowFactor + ambient) * matColor.rgb * textureColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity;
    
    // Multiple Point Light Contributions
    const uint pointLightCount = min(gDirectionalLight.pointLightCount, MAX_POINT_LIGHTS);
    for (uint lightIndex = 0; lightIndex < pointLightCount; ++lightIndex) {
        PointLight pointLight = gDirectionalLight.pointLights[lightIndex];
        float3 plDir = pointLight.position - input.worldPosition;
        float plDist = length(plDir);
        if (pointLight.intensity > 0.0f && plDist < pointLight.radius) {
            plDir /= max(plDist, 0.0001f);
            float plNdotL = max(0.0f, dot(normalize(input.normal), plDir));
            float plAtten = saturate(1.0f - (plDist / pointLight.radius));
            plAtten *= plAtten;
            float3 plContrib = pointLight.color.rgb * plNdotL * plAtten * pointLight.intensity;
            diffuseColor += plContrib * matColor.rgb * textureColor.rgb;
        }
    }
    
    // 2. Specular
    float3 viewDir = normalize(gDirectionalLight.cameraPosition - input.worldPosition);
    float3 lightDir = normalize(-gDirectionalLight.direction);
    float3 halfDir = normalize(lightDir + viewDir);
    
    float specPower = lerp(8.0f, 256.0f, matShininess);
    float specular = pow(saturate(dot(normalize(input.normal), halfDir)), specPower);
    
    float3 specBaseColor = lerp(float3(1.0f, 1.0f, 1.0f), matColor.rgb, matMetallic);
    float specIntensity = lerp(0.15f, 0.8f, matShininess);
    
    float3 specColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specular * specBaseColor * specIntensity * shadowFactor;
    
    // 3. Rim
    float rim = pow(1.0f - saturate(dot(normalize(input.normal), viewDir)), 4.0f);
    float3 rimColor = float3(1.0f, 1.0f, 1.0f) * rim * (0.25f + matEmissive * 0.5f) * gDirectionalLight.intensity;
    
    // 4. Emission
    float3 emissiveColor = matColor.rgb * matEmissive;
    
    output.color.rgb = diffuseColor + specColor + rimColor + emissiveColor;
    output.color.a = matColor.a * textureColor.a;
    
    return output;
}
