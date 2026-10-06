Texture2D<float4> SceneColorTexture : register(t0);

SamplerState LinearClamp : register(s0);

cbuffer FXAAConstants : register(b0)
{
    float2 InverseScreenSize; // { 1.0f / ScreenSize.x, 1.0f / ScreenSize.y }
    float FXAAThreshold; // 0.125f
    float FXAAThresholdMin; // 0.0625f
};

struct FVertexOutput
{
    float4 Position : SV_Position;
    float2 Uv : TEXCOORD0;
};

FVertexOutput mainVS(uint VertexID : SV_VertexID)
{
    float2 Uv = { (VertexID << 1) & 2, VertexID & 2 };
    FVertexOutput Out = { { Uv.x * 2.0f - 1.0f, 1.0f - Uv.y * 2.0f, 0.0f, 1.0f }, Uv };

    return Out;
}



// ------------------------------------------------------------
// FXAA (Fast Approximate Anti-Aliasing)
// ------------------------------------------------------------

float GetLuminance(float3 Color)
{
    return dot(Color, float3(0.299f, 0.587f, 0.114f));
}

float SampleLuminance(float2 uv)
{
    return GetLuminance(SceneColorTexture.Sample(LinearClamp, uv).rgb);
}

float4 FXAA(float2 uv)
{
    float4 ColorM = SceneColorTexture.Sample(LinearClamp, uv);
    float LuminanceM = SampleLuminance(uv);
    
    float LuminanceN = SampleLuminance(uv + float2(0.0f, -InverseScreenSize.y));
    float LuminanceS = SampleLuminance(uv + float2(0.0f, InverseScreenSize.y));
    float LuminanceW = SampleLuminance(uv + float2(-InverseScreenSize.x, 0.0f));
    float LuminanceE = SampleLuminance(uv + float2(InverseScreenSize.x, 0.0f));
    
    float LuminanceMin = min(LuminanceM, min(min(LuminanceN, LuminanceS), min(LuminanceW, LuminanceE)));
    float LuminanceMax = max(LuminanceM, max(max(LuminanceN, LuminanceS), max(LuminanceW, LuminanceE)));
    
    float LuminanceRange = LuminanceMax - LuminanceMin;
    
    float EdgeThreshold = max(FXAAThresholdMin, LuminanceMax * FXAAThreshold);
    
    if (LuminanceRange < EdgeThreshold)
    {
        return ColorM;
    }
    
    float LuminanceNW = SampleLuminance(uv + float2(-InverseScreenSize.x, -InverseScreenSize.y));
    float LuminamceNE = SampleLuminance(uv + float2(InverseScreenSize.x, -InverseScreenSize.y));
    
    float LuminanceSW = SampleLuminance(uv + float2(-InverseScreenSize.x, InverseScreenSize.y));
    float LuminanceSE = SampleLuminance(uv + float2(InverseScreenSize.x, InverseScreenSize.y));
    
    float LuminanceGradientX = ((LuminanceNW + LuminamceNE) - (LuminanceSW + LuminanceSE));
    
    
    
    return float4(1.f, 0.f, 0.f, 0.f);
}

float4 mainPS(FVertexOutput In) : SV_Target
{
    return FXAA(In.Uv);
}