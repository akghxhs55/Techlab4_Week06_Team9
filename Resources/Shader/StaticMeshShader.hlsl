#pragma pack_matrix(row_major)

// ACES 필름 톤 매핑 근사 함수
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 ACESFitted(float3 color)
{
    const float3x3 ACESInputMat =
    {
        { 0.59719, 0.35458, 0.04823 },
        { 0.07600, 0.90834, 0.01566 },
        { 0.02840, 0.13383, 0.83777 }
    };
    const float3x3 ACESOutputMat =
    {
        { 1.60475, -0.53108, -0.07367 },
        { -0.10208, 1.10813, -0.00605 },
        { -0.00327, -0.07276, 1.07602 }
    };
    float3 v = mul(ACESInputMat, color);
    
    float3 a = v * (v + 0.0245786f) - 0.000090537f;
    float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    float3 c = a / b;
    return saturate(mul(ACESOutputMat, c));
}

cbuffer Viewconstants : register(b0)
{
    matrix VP;
    float3 CameraPosition;
    float ViewPadding;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
};

cbuffer MaterialParams : register(b1)
{
    float4 BaseColor;
    float2 UVOffset;
    float bOpaque;
    float Shininess;
};

#define MAX_POINT_LIGHTS 10

struct FPointLightShaderData
{
    float3 Position;
    float AttenuationRadius;
    float4 Color;
    float Intensity;
    float Falloff;
    float bEnabled;
    float Padding;
};

cbuffer PointLightParams : register(b3)
{
    FPointLightShaderData PointLights[MAX_POINT_LIGHTS];
    int NumPointLights;
    float3 LightBufferPadding;
};

struct VS_INPUT
{
    float3 p : POSITION; // Input position from vertex buffer
    float3 n : NORMAL;
    float4 c : COLOR; // Input color from vertex buffer
    float2 t : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD1;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD0;
};

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);

// 기본 방향성 라이트 및 주변광
static const float3 LightDir = normalize(float3(0.5f, 0.5f, -1.0f));
static const float3 LightColor = float3(0.4f, 0.4f, 0.4f);
static const float3 AmbientColor = float3(0.3f, 0.3f, 0.3f);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(float4(input.p, 1.0f), World);
    output.position = mul(worldPos, VP);
    output.worldPos = worldPos.xyz;
    output.normal = normalize(mul(float4(input.n, 0.0f), World).xyz);
    output.color = input.c;
    output.uv = input.t;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_txColor.Sample(g_Sample, input.uv + UVOffset);
    float4 albedo = texColor * BaseColor;

    // 보간되면 길이가 틀어지므로 다시 정규화
    float3 N = normalize(input.normal);

    // 카메라 시선 단위 벡터 (물체 표면 -> 카메라)
    float3 V = normalize(CameraPosition - input.worldPos);
    float specPower = max(Shininess, 1.0f);

    // Ambient
    float3 totalDiffuse = AmbientColor;
    float3 totalSpecular = float3(0.0f, 0.0f, 0.0f);

    // Directional Light
    float3 dirL = -LightDir;
    float dirNdotL = saturate(dot(N, dirL));
    if (dirNdotL > 0.0f)
    {
        float3 dirH = normalize(dirL + V);
        float dirNdotH = saturate(dot(N, dirH));
        float dirSpec = pow(dirNdotH, specPower);

        totalDiffuse += LightColor * dirNdotL;
        totalSpecular += LightColor * dirSpec;
    }

    // 포인트 라이트
    for (int i = 0; i < NumPointLights; ++i)
    {
        if (PointLights[i].bEnabled < 0.5f)
        {
            continue;
        }

        float3 toLight = PointLights[i].Position - input.worldPos;
        float dist = length(toLight);
        float radius = max(PointLights[i].AttenuationRadius, 0.001f);

        if (dist < radius)
        {
            float3 L = toLight / dist;
            float NdotL = saturate(dot(N, L));

            float att = saturate(1.0f - (dist / radius));
            att = pow(att, max(PointLights[i].Falloff, 0.01f));

            float3 lightColor = PointLights[i].Color.rgb * PointLights[i].Intensity;

            // Diffuse
            float3 diffuse = lightColor * NdotL;

            // Specular
            float3 specular = float3(0.0f, 0.0f, 0.0f);
            if (NdotL > 0.0f)
            {
                float3 H = normalize(L + V);
                float NdotH = saturate(dot(N, H));
                specular = lightColor * pow(NdotH, specPower);
            }

            totalDiffuse += diffuse * att;
            totalSpecular += specular * att;
        }
    }

    // Opaque는 알파를 1로 고정한다. 뷰포트 RT를 ImGui가 알파 블렌딩으로 그리므로 알파가 남으면 비쳐 보인다
    float alpha = bOpaque > 0.5f ? 1.0f : albedo.a;
    float3 finalRgb = albedo.rgb * totalDiffuse + totalSpecular;
    finalRgb = ACESFitted(finalRgb);
    finalRgb = pow(finalRgb, 1.0f / 2.2f);
    return float4(finalRgb, alpha);
}
