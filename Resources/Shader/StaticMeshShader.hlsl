#pragma pack_matrix(row_major)
cbuffer Viewconstants : register(b0)
{
    matrix VP;
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
    float Padding;
};

#define MAX_POINT_LIGHTS 4

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

    // 보간되면 길이가 틀어지므로 다시 정규화한다
    float3 N = normalize(input.normal);

    // 기본 환경광 + 방향성 광원 (씬 기본 시인성)
    float dirNdotL = saturate(dot(N, -LightDir));
    float3 totalLighting = AmbientColor + LightColor * dirNdotL;

    // 포인트 라이트 누적 계산
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

            // 부드러운 거리 감쇄 (Falloff)
            float att = saturate(1.0f - (dist / radius));
            att = pow(att, max(PointLights[i].Falloff, 0.01f));

            float3 lightContrib = PointLights[i].Color.rgb * PointLights[i].Intensity * NdotL * att;
            totalLighting += lightContrib;
        }
    }

    // Opaque는 알파를 1로 고정한다. 뷰포트 RT를 ImGui가 알파 블렌딩으로 그리므로 알파가 남으면 비쳐 보인다
    float alpha = bOpaque > 0.5f ? 1.0f : albedo.a;
    return float4(albedo.rgb * totalLighting, alpha);
}
