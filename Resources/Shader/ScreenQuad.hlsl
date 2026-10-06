cbuffer FogCB : register(b0)
{
    row_major float4x4 InverseViewProjection;

    float3 CameraWorldPosition;
    float _Padding0;

    float FogDensity;
    float FogHeightFalloff;
    float FogHeightStart;
    float FogMaxOpacity;

    float3 FogInscatteringLuminance;
    float _Padding1;

    float StartDistance;
    float EndDistance;
    float FogCutOffDistance;
    float _Padding2;
};

Texture2D<float4> SceneColorTexture : register(t0);
Texture2D<float> SceneDepthTexture : register(t1);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);


struct VS_OUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};


// ------------------------------------------------------------
// Fullscreen Triangle
// ------------------------------------------------------------

VS_OUT mainVS(uint VertexID : SV_VertexID)
{
    VS_OUT Out;

    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);

    Out.UV = UV;
    Out.Position = float4(UV.x * 2.0f - 1.0f, 1.0f - UV.y * 2.0f, 0.0f, 1.0f);

    return Out;
}


// ------------------------------------------------------------
// World Position Reconstruction
// ------------------------------------------------------------

float3 ReconstructWorldPosition(float2 UV, float Depth)
{
    // ------------------------------------------------------------
    // 1. Screen UV [0, 1] -> DirectX NDC [-1, 1]
    //
    // Screen UV:
    //
    //      (0,0) -------- (1,0)
    //        |              |
    //        |              |
    //      (0,1) -------- (1,1)
    //
    // DirectX NDC:
    //
    //      (-1,+1) ------ (+1,+1)
    //         |               |
    //         |               |
    //      (-1,-1) ------ (+1,-1)
    //
    // X는 [0,1] -> [-1,1] 로 변환하고,
    // Y는 Texture UV와 NDC의 증가 방향이 반대이므로 뒤집는다.
    // ------------------------------------------------------------

    float2 NDC;
    NDC.x = UV.x * 2.0f - 1.0f;
    NDC.y = 1.0f - UV.y * 2.0f;


    // ------------------------------------------------------------
    // 2. NDC Position 구성
    //
    // DirectX의 NDC Z 범위는 [0, 1].
    //
    // Normal-Z:
    //      Near -> 0
    //      Far  -> 1
    //
    // Reverse-Z:
    //      Near -> 1
    //      Far  -> 0
    //
    // Reverse-Z라고 해서 Depth를 1 - Depth로 변환하지 않는다.
    // Depth를 생성한 ViewProjection의 역행렬을 사용하면
    // 동일한 방법으로 World Position을 복원할 수 있다.
    // ------------------------------------------------------------

    float4 NDCPosition = float4(NDC, Depth, 1.0f);
    float4 WorldPosition = mul(NDCPosition, InverseViewProjection);


    // ------------------------------------------------------------
    // 3. Homogeneous Divide
    //
    // World -> View -> Clip -> NDC 과정의 역변환 결과는
    // homogeneous coordinate이므로 마지막에 w로 나눈다.
    // ------------------------------------------------------------

    return WorldPosition.xyz / WorldPosition.w;
}


// ------------------------------------------------------------
// Exponential Height Fog
//
// Density:
//
// rho(z) = D * exp(-K * (z - H))
//
// D = FogDensity
// K = FogHeightFalloff
// H = FogHeightStart
//
//
// Ray:
//
// P(s) = P0 + Direction * s
//
//
// Optical Depth:
//
// tau = Integral rho(P(s)) ds
//
//     = rho0 * L * (1 - exp(-X)) / X
//
// X = K * Direction.z * L
//
//
// Beer-Lambert:
//
// T = exp(-tau)
//
// FogAmount = 1 - T
// ------------------------------------------------------------

float ComputeExponentialHeightFog(float3 RayDirection, float SurfaceDistance)
{
    // --------------------------------------------------------
    // StartDistance 이전에는 Fog가 존재하지 않는다.
    // --------------------------------------------------------

    if (SurfaceDistance <= StartDistance)
    {
        return 0.0f;
    }


    // --------------------------------------------------------
    // 적분 종료 거리
    //
    // Surface가 EndDistance보다 멀리 있어도
    // Fog 적분은 EndDistance까지만 수행한다.
    // --------------------------------------------------------

    float IntegrationEndDistance = SurfaceDistance;

    if (EndDistance > 0.0f)
    {
        IntegrationEndDistance = min(IntegrationEndDistance, EndDistance);
    }

    if (IntegrationEndDistance <= StartDistance)
    {
        return 0.0f;
    }


    // --------------------------------------------------------
    // 실제 Fog 적분 길이
    //
    // L = IntegrationEndDistance - StartDistance
    // --------------------------------------------------------

    float IntegrationDistance = IntegrationEndDistance - StartDistance;


    // --------------------------------------------------------
    // 실제 Fog 적분 시작 위치
    //
    // P0 = Camera + RayDirection * StartDistance
    // --------------------------------------------------------

    float3 FogStartPosition = CameraWorldPosition + RayDirection * StartDistance;


    // --------------------------------------------------------
    // 적분 시작 지점의 밀도
    //
    // rho0 = D * exp(-K * (z0 - H))
    // --------------------------------------------------------

    float StartDensity = FogDensity * exp(-FogHeightFalloff * (FogStartPosition.z - FogHeightStart));


    // --------------------------------------------------------
    // 적분 구간 동안의 높이 변화
    //
    // DeltaZ = RayDirection.z * L
    //
    // X = K * DeltaZ
    // --------------------------------------------------------

    float HeightDelta = RayDirection.z * IntegrationDistance;
    float X = FogHeightFalloff * HeightDelta;


    // --------------------------------------------------------
    // Optical Depth 계산
    //
    // tau = rho0 * L * (1 - exp(-X)) / X
    //
    // X -> 0이면:
    //
    // lim (1 - exp(-X)) / X = 1
    // --------------------------------------------------------

    float OpticalDepth;

    if (abs(X) < 1e-4f)
    {
        OpticalDepth = StartDensity * IntegrationDistance;
    }
    else
    {
        OpticalDepth = StartDensity * IntegrationDistance * (1.0f - exp(-X)) / X;
    }


    // --------------------------------------------------------
    // Beer-Lambert Law
    //
    // T = exp(-tau)
    //
    // FogAmount = 1 - T
    // --------------------------------------------------------

    float Transmittance = exp(-OpticalDepth);
    float FogAmount = 1.0f - Transmittance;

    return min(FogAmount, saturate(FogMaxOpacity));
}


// ------------------------------------------------------------
// Pixel Shader
// ------------------------------------------------------------

#define EPSILON 1e-6f

float4 mainPS(VS_OUT Input) : SV_Target
{
    float4 SceneColor = SceneColorTexture.Sample(LinearSampler, Input.UV);
    float Depth = SceneDepthTexture.Sample(PointSampler, Input.UV);


    // --------------------------------------------------------
    //
    // Normal-Z Background Test
    //
    // Near  -> 0
    // Far   -> 1
    // Clear -> 1
    //
    // Depth가 거의 1이면 Geometry가 없는 영역으로 간주한다.
    // 현재는 Sky에는 Fog를 적용하지 않는다.
    //
    // --------------------------------------------------------

    if (Depth >= 1.0f - EPSILON)
    {
        return SceneColor;
    }


    // --------------------------------------------------------
    // World Position 복원
    // --------------------------------------------------------

    float3 WorldPosition = ReconstructWorldPosition(Input.UV, Depth);


    // --------------------------------------------------------
    // Camera -> Surface Ray
    // --------------------------------------------------------

    float3 CameraToSurface = WorldPosition - CameraWorldPosition;
    float SurfaceDistance = length(CameraToSurface);

    if (SurfaceDistance <= EPSILON)
    {
        return SceneColor;
    }


    // --------------------------------------------------------
    // FogCutOffDistance
    //
    // 이 거리보다 먼 Surface에는 Height Fog를 적용하지 않는다.
    //
    // EndDistance:
    //      Fog 밀도 적분을 어디까지 수행할지 결정
    //
    // FogCutOffDistance:
    //      해당 Surface 자체에 Fog를 적용할지 결정
    // --------------------------------------------------------

    if (FogCutOffDistance > 0.0f && SurfaceDistance > FogCutOffDistance)
    {
        return SceneColor;
    }


    float3 RayDirection = CameraToSurface / SurfaceDistance;


    // --------------------------------------------------------
    // Height Fog
    // --------------------------------------------------------

    float FogAmount = ComputeExponentialHeightFog(RayDirection, SurfaceDistance);


    // --------------------------------------------------------
    // Fog Composition
    //
    // Cout = Cscene * T + Cfog * (1 - T)
    //
    // FogAmount = 1 - T
    //
    // 따라서:
    //
    // Cout = lerp(Cscene, Cfog, FogAmount)
    // --------------------------------------------------------

    float3 FinalColor = lerp(SceneColor.rgb, FogInscatteringLuminance, FogAmount);

    return float4(FinalColor, SceneColor.a);
}