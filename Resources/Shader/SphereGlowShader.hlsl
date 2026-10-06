#pragma pack_matrix(row_major)

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
    float4 position     : SV_POSITION;
    float3 worldPos     : TEXCOORD0;
    float3 camPosOrDir  : TEXCOORD1;
    float4 color        : COLOR;
    float2 uv           : TEXCOORD2;
    float  isPerspective: TEXCOORD3;
};

// 구체 버텍스 위치를 world 및 viewproj로 변환하고, 카메라 위치/방향을 픽셀 셰이더로 전달
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    float4 localPos = float4(input.p * Radius, 1.0f);
    float4 worldPos = mul(localPos, World);
    
    output.position = mul(worldPos, VP);
    output.worldPos = worldPos.xyz;
    output.color = input.c;
    output.uv = input.t;
    
    // VP 행렬의 4번째 열(Column 3)의 길이로 원근(Perspective) vs 직교(Orthographic) 투영 판별
    float3 forwardP = float3(VP[0][3], VP[1][3], VP[2][3]);
    float pLenSq = dot(forwardP, forwardP);
    
    if (pLenSq > 0.25f)
    {
        // 원근 투영
        float3 N1 = float3(VP[0][0], VP[1][0], VP[2][0]);
        float3 N2 = float3(VP[0][1], VP[1][1], VP[2][1]);
        float3 N3 = forwardP;
        
        float d1 = VP[3][0];
        float d2 = VP[3][1];
        float d3 = VP[3][3];
        
        float3 c23 = cross(N2, N3);
        float det = dot(N1, c23);
        float3 c31 = cross(N3, N1);
        float3 c12 = cross(N1, N2);
        
        output.camPosOrDir = (-d1 * c23 - d2 * c31 - d3 * c12) / det;
        output.isPerspective = 1.0f;
    }
    else
    {
        // 직교 투영
        // 3번째 열(Column 2)이 카메라의 실제 전방 방향 (Forward * ZScale)
        // ZScale = 1.0 / (Far - Near) 이므로 0.001보다 작을 수 있어 1e-12f로 안전하게 정규화
        float3 forwardO = float3(VP[0][2], VP[1][2], VP[2][2]);
        float oLenSq = dot(forwardO, forwardO);
        output.camPosOrDir = (oLenSq > 1e-12f) ? (forwardO * rsqrt(oLenSq)) : float3(0.0f, 0.0f, 1.0f);
        output.isPerspective = 0.0f;
    }
    
    return output;
}

// 픽셀 시선 레이(Ray)와 구체 중심 간의 수직 거리를 계산하여 감쇠를 적용
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float distance = 0.0f;
    
    if (input.isPerspective > 0.5f)
    {
        // 원근 투영: 각 픽셀마다 카메라 위치에서 픽셀 월드 좌표로 향하는 실제 시선 레이(Ray) 계산
        float3 camPos = input.camPosOrDir;
        float3 toPixel = input.worldPos - camPos;
        float len = length(toPixel);
        float3 rayDir = (len > 0.0001f) ? (toPixel / len) : float3(0.0f, 0.0f, 1.0f);
        
        float3 toCenter = Center - camPos;
        // 시선 레이를 따라 구체 중심을 투영 (카메라 뒤쪽으로 투영되지 않도록 t >= 0 제한)
        float t = max(dot(toCenter, rayDir), 0.0f);
        
        // 레이 상에서 구체 중심과 가장 가까운 지점까지의 수직 거리 계산
        float3 closestPoint = camPos + t * rayDir;
        distance = length(Center - closestPoint);
    }
    else
    {
        // 직교 투영: 모든 시선이 평행하므로 카메라 전방 축과의 수직 거리 계산
        float3 viewForward = input.camPosOrDir;
        float3 offset = input.worldPos - Center;
        float3 perp = offset - dot(offset, viewForward) * viewForward;
        distance = length(perp);
    }
    
    // 감쇠 계산: 중심(distance = 0)에서 최대, 가장자리(distance = Radius) 부근에서 RadiusFallOff 두께로 부드럽게 감쇠
    float attenuation = saturate((Radius - distance) / max(RadiusFallOff, 0.001f));
    // 지수 감쇠: 중심 코어는 단단하고 쨍하게 유지하고, 외곽은 가파르게 옅어지도록 처리
    attenuation = pow(attenuation, 2.5f);

    // 1. 순수 고광도(HDR) 색상 계산
    float3 hdrColor = Color.rgb * Intensity * attenuation;
    // 2. 필름 톤매핑 통과 -> 강한 중심부는 자동으로 눈부신 백색으로, 외곽은 원색으로!
    float3 tonemappedColor = ACESFitted(hdrColor);
    return float4(tonemappedColor, Color.a * attenuation);
}