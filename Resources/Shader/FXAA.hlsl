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
    
    
    //========================================================
    // 1. 현재 픽셀 + 상하좌우 샘플
    //
    //        N
    //
    //    W   M   E
    //
    //        S
    //========================================================
    float LuminanceM = SampleLuminance(uv);
    float LuminanceN = SampleLuminance(uv + float2(0.0f, -InverseScreenSize.y));
    float LuminanceS = SampleLuminance(uv + float2(0.0f, InverseScreenSize.y));
    float LuminanceW = SampleLuminance(uv + float2(-InverseScreenSize.x, 0.0f));
    float LuminanceE = SampleLuminance(uv + float2(InverseScreenSize.x, 0.0f));
    
    
    //========================================================
    // 2. Contrast 검사
    //
    // contrast가 충분히 크지 않으면
    // AA가 필요 없는 픽셀이라고 판단하고 바로 종료한다.
    //========================================================
    float LuminanceMin = min(LuminanceM, min(min(LuminanceN, LuminanceS), min(LuminanceW, LuminanceE)));
    float LuminanceMax = max(LuminanceM, max(max(LuminanceN, LuminanceS), max(LuminanceW, LuminanceE)));
    
    float LuminanceRange = LuminanceMax - LuminanceMin;
    
    float EdgeThreshold = max(FXAAThresholdMin, LuminanceMax * FXAAThreshold);
    
    
    if (LuminanceRange < EdgeThreshold)
    {
        return ColorM;
    }
    
    // return float4(1.f, 0.f, 0.f, 0.f);
    
    
    
    //========================================================
    // 3. 대각선까지 추가 샘플
    //
    //    NW   N   NE
    //
    //     W   M    E
    //
    //    SW   S   SE
    //========================================================
    float LuminanceNW = SampleLuminance(uv + float2(-InverseScreenSize.x, -InverseScreenSize.y));
    float LuminanceNE = SampleLuminance(uv + float2(InverseScreenSize.x, -InverseScreenSize.y));
    
    float LuminanceSW = SampleLuminance(uv + float2(-InverseScreenSize.x, InverseScreenSize.y));
    float LuminanceSE = SampleLuminance(uv + float2(InverseScreenSize.x, InverseScreenSize.y));
    
    
    
    //========================================================
    // 4. Edge 방향 판정
    //
    // Edge가 가로로 놓였는지 세로로 놓였는지를 판단한다.
    //
    // 여기서 주의할 점:
    //
    // EdgeHorizontal가 크다
    //      -> edge 자체가 수평
    //      -> gradient는 수직 방향으로 강하다.
    //
    //
    //       밝음
    //  -------------
    //       어두움
    //
    //       ↑ gradient
    //       │
    //       │
    //
    // edge 진행 방향은 ← →
    //========================================================
    float EdgeHorizontal = abs(-2.0f * LuminanceM + LuminanceN + LuminanceS) * 2.f + abs(-2.0f * LuminanceE + LuminanceNE + LuminanceSE) + abs(-2.0f * LuminanceW + LuminanceNW + LuminanceSW);
    float EdgeVertical = abs(-2.0f * LuminanceM + LuminanceE + LuminanceW) * 2.f + abs(-2.0f * LuminanceN + LuminanceNE + LuminanceNW) + abs(-2.0f * LuminanceS + LuminanceSE + LuminanceSW);
    
    bool IsHorizontal = EdgeHorizontal >= EdgeVertical;
    
   // return IsHorizontal ? float4(0.f,1.f,0.f,1.f) : float4(1.f,0.f,0.f,1.f);
    
    
    //========================================================
    // 5. Edge의 어느 쪽에 현재 픽셀이 있는지 결정
    //
    // 수평 edge:
    //
    //       N
    //       │
    //       M
    //       │
    //       S
    //
    // N-M gradient와 M-S gradient 중
    // 더 강한 쪽을 고른다.
    //
    // 세로 edge라면 W/E를 비교한다.
    //========================================================
    float luma1 = IsHorizontal ? LuminanceN : LuminanceW;
    float luma2 = IsHorizontal ? LuminanceS : LuminanceE;
    
    
    float Gradient1 = abs(luma1 - LuminanceM);
    float Gradient2 = abs(luma2 - LuminanceM);
    
    bool Is1Steepest = Gradient1 >= Gradient2;
    float Gradient = max(Gradient1, Gradient2);
    
    
    // Edge에 수직인 방향
    //
    // Horizontal Edge -> Y축 이동
    // Vertical Edge   -> X축 이동
    float2 PixelStep = IsHorizontal ? float2(0.0f, InverseScreenSize.y) : float2(InverseScreenSize.x, 0.0f);

    float LumaLocalAvg; 
    
    // N 또는 W 쪽 Gradient 우세 
    if (Is1Steepest)
    {
        PixelStep = -PixelStep;
        LumaLocalAvg = 0.5f * (LuminanceM + luma1);
        
    }
    else
    {
        LumaLocalAvg = 0.5f * (LuminanceM + luma2);
    }
    
    
    //========================================================
    // 현재 픽셀에서 gradient 방향으로 절반 이동
    //
    //
    // Pixel center
    //       ●
    //       │
    //       │ 0.5 pixel
    //       ↓
    // ------X------ edge 중심
    //
    // 이 위치부터 edge를 따라 양방향 탐색한다.
    //========================================================
    float2 EdgeCenterUv = uv + 0.5f * PixelStep;
    
    float2 EdgeDir = IsHorizontal ? float2(InverseScreenSize.x, 0.0f) : float2(0.0f, InverseScreenSize.y);
    
    
    
    //========================================================
    // 6. Endpoint 탐색
    //========================================================
    float GradientTreshold = Gradient * 0.25f;
    
    
    static const float SearchDistances[12] =
    {
        1.0f,
        1.5f,
        2.0f,
        2.5f,
        3.0f,
        4.0f,
        5.0f,
        6.0f,
        8.0f,
        10.0f,
        12.0f,
        16.0f
    };
    
    float2 UVNegative = EdgeCenterUv;
    float2 UVPositive = EdgeCenterUv;
    
    float DeltaNegative = 0.0f;
    float DeltaPositive = 0.0f;
    
    bool ReachedNegativeEnd = false;
    bool ReachedPositiveEnd = false;
    
    [unroll]
    for (uint i = 0; i < 12; ++i)
    {
        float SearchDistance = SearchDistances[i];
        
        // Edge 진행 방향의 양쪽을 탐색
        float2 CandidateUVNegative = EdgeCenterUv - SearchDistance * EdgeDir;
        float2 CandidateUVPositive = EdgeCenterUv + SearchDistance * EdgeDir;
        
        if (!ReachedNegativeEnd)
        {
            UVNegative = CandidateUVNegative;
            
            float LumaNegative = SampleLuminance(UVNegative);
            
            DeltaNegative = LumaNegative - LumaLocalAvg;
            
            // 시작 지점과 충분히 달라졌다면
            // edge가 변화한 지점으로 판단한다.
            ReachedNegativeEnd = (abs(DeltaNegative) >= GradientTreshold);
        }
        
        if (!ReachedPositiveEnd)
        {
            UVPositive = CandidateUVPositive;
            
            float LumaPositive = SampleLuminance(UVPositive);
            
            DeltaPositive = LumaPositive - LumaLocalAvg;
            
            ReachedPositiveEnd = (abs(DeltaPositive) >= GradientTreshold);
        }
        
        if (ReachedNegativeEnd && ReachedPositiveEnd)
        {
            break;
        }
    }
    
    
    //========================================================
    // 7. 현재 픽셀과 양쪽 endpoint의 거리
    //
    //
    // endpoint A           endpoint B
    //
    //     |-------------------|
    //              ^
    //              M
    //
    // 어느 endpoint가 현재 픽셀에 가까운지 계산한다.
    //========================================================
    float DistanceNegative; 
    float DistancePositive; 
    
    if (IsHorizontal)
    {
        DistanceNegative = uv.x - UVNegative.x;
        DistancePositive = UVPositive.x - uv.x;
    }
    else
    {
        DistanceNegative = uv.y - UVNegative.y;
        DistancePositive = UVPositive.y - uv.y;
    }
    
    bool IsNegativeNearest = DistanceNegative < DistancePositive;
    float DistanceFinal = min(DistanceNegative, DistancePositive);
    
    float EdgeLength = DistanceNegative + DistancePositive;
    
    
    //========================================================
    // 8. Edge 기반 pixel offset 계산
    //
    // endpoint 사이에서 현재 픽셀이 어느 정도
    // 치우쳐 있는지 계산한다.
    //
    //
    // endpoint         M                    endpoint
    //    |-------------|-----------------------|
    //
    // M이 endpoint에 가까울수록
    // 더 큰 AA offset이 필요하다.
    //========================================================
    float EdgePixelOffset = 0.f;
    
    if (EdgeLength > 1e-6f)
    {
        float PixelOffset = 0.5f - DistanceFinal / EdgeLength;
        
        // 가까운 endpoint의 luminance 변화 방향
        float EndPointDelta = IsNegativeNearest ? DeltaNegative : DeltaPositive;
        
        EdgePixelOffset = PixelOffset;
    }
    
    
    //========================================================
    // 9. Subpixel Alias 제거
    //
    // 긴 edge가 아니라,
    // 한두 픽셀 수준의 작은 aliasing에도 대응한다.
    //========================================================
    float LumaAvg = (2.f * (LuminanceN + LuminanceS + LuminanceW + LuminanceE) + LuminanceNW + LuminanceNE + LuminanceSW + LuminanceSE) / 12.f;
    float SubPixelOffset = saturate(abs(LumaAvg - LuminanceM) / max(LuminanceRange, 1e-6f));
    SubPixelOffset = SubPixelOffset * SubPixelOffset * ( 3.f - 2.f * SubPixelOffset);
    SubPixelOffset *= 0.75f; // Subpixel Quality
    
    
    //========================================================
    // 10. Edge AA와 Subpixel AA 중 더 강한 것을 선택
    //========================================================
    float2 FinalOffset = max(EdgePixelOffset, SubPixelOffset);
    
    //========================================================
    // 11. 최종 UV
    //
    // 중요한 부분:
    //
    // edge를 따라 이동하는 게 아니다.
    //
    // edge endpoint를 찾기 위해서는 edge 방향으로 탐색했지만,
    //
    // 최종 AA sampling은
    //
    //        edge와 "수직"
    //
    // 인 방향으로 이동한다.
    //========================================================
    float2 FinalUV = uv + FinalOffset * PixelStep;
    
    float3 FinalColor = SceneColorTexture.Sample(LinearClamp, FinalUV).rgb;
    
    return float4(FinalColor, 1.f);
    
}

float4 mainPS(FVertexOutput In) : SV_Target
{
    return FXAA(In.Uv);
}