cbuffer FogCB : register(b0) {
    row_major float4x4 InverseViewProjection;

    float3 CameraWorldPosition;
    float FogDensity;

    float3 FogInscatteringColor;
    float FogHeight;

    float FogHeightFalloff;
    float FogStartDistance;
    float FogMaxOpacity;
    float FogMaxDistance;
    
    float FogCutOffDistance;
    float3 _Padding0;
};

Texture2D<float4> SceneColorTexture : register(t0);
Texture2D<float> SceneDepthTexture : register(t1);

SamplerState LinearSampler : register(s0);


struct FVertexOutput {
    float4 mPosition : SV_Position;
    float2 mUv : TEXCOORD0;
};


// ------------------------------------------------------------
// Fullscreen Triangle
// ------------------------------------------------------------

FVertexOutput mainVS(uint VertexID : SV_VertexID) {
    float2 Uv = { (VertexID << 1) & 2, VertexID & 2 };
    FVertexOutput Out = { { Uv.x * 2.0f - 1.0f, 1.0f - Uv.y * 2.0f, 0.0f, 1.0f }, Uv };

    return Out;
}


float3 ReconstructWorldPosition(float2 Uv, float Depth) {
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
    // 따라서 X는 [0,1] -> [-1,1] 로 변환하고,
    // Y는 Texture UV의 아래쪽 증가 방향과 NDC의 위쪽 증가 방향이
    // 서로 반대이므로 뒤집어 준다.
    // ------------------------------------------------------------
    float2 Ndc = { Uv.x * 2.0f - 1.0f, 1.0f - Uv.y * 2.0f };
    
    
     // ------------------------------------------------------------
    // 2. NDC 공간 위치 구성
    //
    // Depth Buffer에는 Projection 이후의 NDC Z 값이 저장되어 있다.
    //
    // DirectX의 NDC Z 범위는:
    //
    //      Z ∈ [0, 1]
    //
    // 따라서 Depth 값을 그대로 Z 성분으로 사용할 수 있다.
    //
    // Normal-Z:
    //      Near -> 0
    //      Far  -> 1
    //
    // Reverse-Z:
    //      Near -> 1
    //      Far  -> 0
    //
    // Reverse-Z라고 해서 여기서 Depth를 1 - Depth로 바꾸면 안 된다.
    //
    // 해당 Depth를 생성한 Projection Matrix의 역행렬을 사용하면
    // Normal-Z / Reverse-Z 모두 동일한 방식으로 역변환할 수 있다.
    // ------------------------------------------------------------

    float4 ClipPosition = { Ndc, Depth, 1.0f };

    float4 WorldPosition = { mul(ClipPosition, InverseViewProjection) };

    
     // ------------------------------------------------------------
    // 3. NDC -> World Homogeneous Position
    //
    // 원래 Vertex가 화면에 투영되는 과정은:
    //
    //      World
    //        ↓ View
    //      View
    //        ↓ Projection
    //      Clip
    //        ↓ Perspective Divide
    //      NDC
    //
    // 즉:
    //
    //      P_clip = P_world * ViewProjection
    //
    // 이 과정을 반대로 수행하기 위해
    // InverseViewProjection을 곱한다.
    //
    // 주의:
    //
    // 현재 Position = (NDC.xyz, 1)은 엄밀히 말하면
    // 원래의 Clip Position 자체는 아니다.
    //
    // 하지만 Projection은 homogeneous transform이므로,
    // 이 값을 역변환하면 결과가 homogeneous 좌표로 나오고,
    // 마지막에 w로 나누면 올바른 World Position을 복원할 수 있다.
    // ------------------------------------------------------------
    
    return WorldPosition.xyz / WorldPosition.w;
}


// ------------------------------------------------------------
// Exponential Height Fog
//
// rho(z) = D * exp(-K * (z - H))
//
// OpticalDepth
//
// tau = Integral rho(s) ds
//
//     = rho0 * L * (1 - exp(-x)) / x
//
// x = K * dz * L
// ------------------------------------------------------------

float ComputeExponentialHeightFog(float3 RayDirection, float SurfaceDistance) {
    // Start Distance 이전에는 Fog가 존재하지 않는다.
    if (FogDensity <= 0.0f || SurfaceDistance <= FogStartDistance) {
        return 0.0f;
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
        return 0.f;
    }

    float IntegrationDistance = { SurfaceDistance - FogStartDistance };

    // 실제 Fog 적분 시작 위치
    float3 FogStartPosition = { CameraWorldPosition + RayDirection * FogStartDistance };

    // rho0 = D * exp(-K(z0 - H))
    float StartExponent = { -FogHeightFalloff * (FogStartPosition.z - FogHeight) };
    float StartDensity = { FogDensity * exp(clamp(StartExponent, -80.0f, 80.0f)) };

    // 적분 구간 동안 변화하는 높이
    // DeltaZ = dz * L
    float HeightDelta = { RayDirection.z * IntegrationDistance };

    float X = { FogHeightFalloff * HeightDelta };

    
    
    // --------------------------------------------------------
    // Optical Depth 계산
    //
    // tau = rho0 * L * (1 - exp(-X)) / X
    //
    // X -> 0이면:
    //
    // lim (1 - exp(-X)) / X = 1
    //
    // --------------------------------------------------------
    float OpticalDepth = { StartDensity * IntegrationDistance };
    if (abs(X) >= 1e-4f) {
        float EndDensity = { FogDensity * exp(clamp(StartExponent - X, -80.0f, 80.0f)) };
        OpticalDepth = (StartDensity - EndDensity) * IntegrationDistance / X;
    }

    // Beer-Lambert
    //
    // T = exp(-tau)

    float Transmittance = { exp(-max(OpticalDepth, 0.0f)) };

    float FogAmount = { 1.0f - Transmittance };

    return min(FogAmount, saturate(FogMaxOpacity));
}


// ------------------------------------------------------------
// Pixel Shader
// ------------------------------------------------------------
static const float Epsilon = { 1e-6f };
float4 mainPS(FVertexOutput Input) : SV_Target {
    float4 SceneColor = { SceneColorTexture.Sample(LinearSampler, Input.mUv) };
    float Depth = { SceneDepthTexture.Load(int3(Input.mPosition.xy, 0)) };

    bool IsSky = { Depth >= 1.0f };
    float ReconstructionDepth = { IsSky && FogMaxDistance > 0.0f ? 0.5f : Depth };


    // --------------------------------------------------------
    // World Position 복원
    // --------------------------------------------------------

    float3 WorldPosition = { ReconstructWorldPosition(Input.mUv, ReconstructionDepth) };


    // --------------------------------------------------------
    // Camera -> Surface Ray
    // --------------------------------------------------------

    float3 CameraToSurface = { WorldPosition - CameraWorldPosition };

    float SurfaceDistance = { length(CameraToSurface) };

    if (SurfaceDistance <= Epsilon) {
        return SceneColor;
    }

    float3 RayDirection = { CameraToSurface / SurfaceDistance };


    // 선택적 최대 거리 제한
    if (FogMaxDistance > 0.0f) {
        SurfaceDistance = IsSky ? FogMaxDistance : min(SurfaceDistance, FogMaxDistance);
    }


    // --------------------------------------------------------
    // Height Fog
    // --------------------------------------------------------

    float FogAmount = { ComputeExponentialHeightFog(RayDirection, SurfaceDistance) };


    // --------------------------------------------------------
    // Fog Composition
    //
    // Cout = Cscene * T + Cfog * (1-T)
    //
    // FogAmount = 1-T
    // --------------------------------------------------------

    float3 FinalColor = { lerp(SceneColor.rgb, FogInscatteringColor, FogAmount) };

    return float4(FinalColor, SceneColor.a );
}
