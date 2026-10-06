cbuffer ScreenQuadCB : register(b0)
{
    row_major float4x4 InverseProj;
};

struct VS_OUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

Texture2D<float> g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

VS_OUT mainVS(uint VertexID : SV_VertexID)
{
    VS_OUT Out;

    // Fullscreen Triangle
    // 0 -> (0, 0)
    // 1 -> (2, 0)
    // 2 -> (0, 2)
    float2 UV = float2(
        (VertexID << 1) & 2,
        VertexID & 2
    );

    Out.UV = UV;

    Out.Position = float4(
        UV.x * 2.0f - 1.0f,
        1.0f - UV.y * 2.0f,
        0.0f,
        1.0f
    );

    return Out;
}

float3 ReconstructViewPosition(float2 UV, float Depth)
{
    // Texture UV -> DirectX NDC
    float2 NDC;
    NDC.x = UV.x * 2.0f - 1.0f;
    NDC.y = 1.0f - UV.y * 2.0f;

    // D3D의 NDC Z는 [0, 1].
    // Depth Buffer에서 읽은 값을 그대로 사용하면 됨.
    float4 ClipPosition = float4(
        NDC.x,
        NDC.y,
        Depth,
        1.0f
    );

    float4 ViewPosition = mul(ClipPosition, InverseProj);

    ViewPosition.xyz /= ViewPosition.w;

    return ViewPosition.xyz;
}

#define EPSILON 1e-6f

float4 mainPS(VS_OUT In) : SV_Target {
    float Depth = { g_Texture.Sample(g_Sampler, In.UV) };
    
    if (Depth == 1.f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float3 ViewPosition = { ReconstructViewPosition(In.UV, Depth) };

    float ViewDepth = { ViewPosition.x }; // Near ~ Far 

    
    float Visualization = { frac(ViewDepth / 50.0f) }; // 50.f 마다 반복 
    
    return float4(Visualization.rrr, 1.0f);
    //return float4(Depth.rrr, 1.f);
}
