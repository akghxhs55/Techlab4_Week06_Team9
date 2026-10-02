#pragma pack_matrix(row_major)
cbuffer Viewconstants : register(b0)
{
    matrix VP;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
};

cbuffer SphereGlowParams : register(b1)
{
    float3 Center;
    float Radius;
    float4 Color;
    float Intensity;
    float RadiusFallOff;
    float2 Padding;
};

struct VS_INPUT
{
    float3 p : POSITION;
    float3 n : NORMAL;
    float4 c : COLOR;
    float2 t : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 viewDir  : TEXCOORD1;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD2;
};

// 구체 버텍스 위치를 world 및 viewproj로 변환하고, 픽셀 셰이더로 WorldPos와 카메라 방향을 전달
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    float4 localPos = float4(input.p * Radius, 1.0f);
    float4 worldPos = mul(localPos, World);
    
    output.position = mul(worldPos, VP);
    output.worldPos = worldPos.xyz;
    // Perspective는 W열(전방), Orthographic은 Z열(전방)에서 안전하게 카메라 방향을 추출
    float3 forwardP = float3(VP[0].w, VP[1].w, VP[2].w);
    float3 forwardO = float3(VP[0].z, VP[1].z, VP[2].z);
    output.viewDir = (length(forwardP) > 0.001f) ? normalize(forwardP) : normalize(forwardO);
    output.color = input.c;
    output.uv = input.t;
    return output;
}

// 픽셀-구체 중심 간의 시선 평면 거리를 계산하고, 감쇠를 적용하여 최종 색상을 출력
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float3 offset = input.worldPos - Center;
    float len = length(input.viewDir);
    float3 viewForward = (len > 0.0001f) ? (input.viewDir / len) : float3(1.0f, 0.0f, 0.0f);
    
    // 시선 방향과 수직인 평면(원형 실루엣 디스크) 상에서 구체 중심으로부터의 수직 거리 계산
    float3 perp = offset - dot(offset, viewForward) * viewForward;
    float distance = length(perp);
    
    // 감쇠 계산: 중심(distance = 0)에서 최대, 가장자리(distance = Radius) 부근에서 RadiusFallOff 두께로 부드럽게 감쇠
    float attenuation = saturate((Radius - distance) / max(RadiusFallOff, 0.001f));
    float4 finalColor = Color * Intensity * attenuation;
    return finalColor;
}