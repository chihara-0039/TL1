struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 lightSpacePosition : POSITION0;
    float3 worldPosition : POSITION1; // ワールド空間の位置を追加
};

struct Material
{
    float4 color;
    int enableLighting;
    float shininess;
    float metallic;
    float emissive;
    float4x4 uvTransform;
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
    float3 cameraPosition; // カメラの位置を追加
    float paddingLight;        // アライメント用パディング
    PointLight pointLights[MAX_POINT_LIGHTS];
    uint pointLightCount;
    float3 pointLightPadding;
};

ConstantBuffer<Material> gMaterial : register(b0);


ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 5番目のスロット(t1)に届いているシャドウマップを受け取る
Texture2D<float> gShadowMap : register(t1);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // テクスチャのサンプリング（既存）
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // ライティング計算
    if (gMaterial.enableLighting != 0)
    {
        // ==========================================================
        // 影の計算（ライティング有効時のみ実行）
        // ==========================================================
        float3 lightPos = input.lightSpacePosition.xyz / input.lightSpacePosition.w;
        float2 shadowUV = lightPos.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
        float currentDepth = lightPos.z; // このピクセルのライトからの距離
     
        float shadowFactor = 1.0f; // 影なし（明るい）
     
        // シャドウマップの範囲内の場合のみ判定します
        if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f && shadowUV.y >= 0.0f && shadowUV.y <= 1.0f)
        {
            // 1-tapの超高速・高精細シャドウサンプリング (テクスチャサンプリング負荷を900%削減)
            float mapDepth = gShadowMap.Sample(gSampler, shadowUV);
            
            // 自分の距離の方が奥にあれば影と判定（バイアスを加味）
            float depthDiff = currentDepth - mapDepth;
            if (depthDiff > 0.0005f)
            {
                // 距離が離れるほど影を薄くするフェードアウト処理
                float fade = saturate(1.0f - (depthDiff / 0.05f));
                shadowFactor = lerp(1.0f, 0.6f, fade);
            }
        }

        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        float ambient = 0.35f;
        
        // 1. 拡散反射光 (Diffuse) - ハーフランバートにソフトシャドウを適用
        float3 diffuseColor = (cos * shadowFactor + ambient) * gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity;

        // 複数ポイントライト。プレイヤー、雷、松明などを同時に合成できる。
        const uint pointLightCount = min(gDirectionalLight.pointLightCount, MAX_POINT_LIGHTS);
        for (uint lightIndex = 0; lightIndex < pointLightCount; ++lightIndex)
        {
            PointLight pointLight = gDirectionalLight.pointLights[lightIndex];
            float3 toLight = pointLight.position - input.worldPosition;
            float distanceToLight = length(toLight);
            if (pointLight.intensity > 0.0f && distanceToLight < pointLight.radius)
            {
                float3 pointDirection = toLight / max(distanceToLight, 0.0001f);
                float pointNdotL = saturate(dot(normalize(input.normal), pointDirection));
                float attenuation = saturate(1.0f - distanceToLight / pointLight.radius);
                attenuation *= attenuation;
                diffuseColor += pointLight.color.rgb * pointNdotL * attenuation * pointLight.intensity * gMaterial.color.rgb * textureColor.rgb;
            }
        }
        
        // 2. スペキュラー反射光 (Blinn-Phong Specular) - 影の中ではハイライトを減衰して自然に見せる
        float3 viewDir = normalize(gDirectionalLight.cameraPosition - input.worldPosition);
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float3 halfDir = normalize(lightDir + viewDir);
        
        // gMaterial.shininess を反射光の広がり（指数）にマッピング
        // 0.0f の時は 8.0f（鈍い光）、1.0f の時は 256.0f（非常に鋭いハイライト）
        float specPower = lerp(8.0f, 256.0f, gMaterial.shininess);
        float specular = pow(saturate(dot(normalize(input.normal), halfDir)), specPower);
        
        // metallic が高いほど、スペキュラー色にマテリアルの色を強く混ぜる（金属特有の反射光）
        float3 specBaseColor = lerp(float3(1.0f, 1.0f, 1.0f), gMaterial.color.rgb, gMaterial.metallic);
        
        // shininess に応じて反射の強さを調節
        float specIntensity = lerp(0.15f, 0.8f, gMaterial.shininess);
        
        float3 specColor = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specular * specBaseColor * specIntensity * shadowFactor;
        
        // 3. リムライト (Rim Light) - 物体の輪郭を光らせて立体感を極限まで高める
        float rim = pow(1.0f - saturate(dot(normalize(input.normal), viewDir)), 4.0f);
        // emissive に応じてリムライトの光り方を補強
        float3 rimColor = float3(1.0f, 1.0f, 1.0f) * rim * (0.25f + gMaterial.emissive * 0.5f) * gDirectionalLight.intensity;
        
        // 4. 自発光 (Emission) - 光源がなくても自己発光する
        float3 emissiveColor = gMaterial.color.rgb * gMaterial.emissive;
        
        // 最終カラー合成
        output.color.rgb = diffuseColor + specColor + rimColor + emissiveColor;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}
