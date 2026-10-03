struct VS_OUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

Texture2D g_Texture : register(t0);

SamplerState g_Sampler : register(s0);

VS_OUT mainVS(uint VertexID : SV_VertexID)
{
    VS_OUT Out;

    // VertexID:
    // 0 -> (0, 0)
    // 1 -> (2, 0)
    // 2 -> (0, 2)
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);

    Out.UV = UV;

    // UV -> Clip Space
    Out.Position = float4(UV.x * 2.0f - 1.0f, 1.0f - UV.y * 2.0f, 0.0f, 1.0f);

    return Out;
}

float4 mainPS(VS_OUT In) : SV_Target
{
    float4 Color = g_Texture.Sample(g_Sampler, In.UV);
    return Color.rgba;
}