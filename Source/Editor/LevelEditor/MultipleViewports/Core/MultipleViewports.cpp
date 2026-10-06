// 다중 뷰포트의 레이아웃·카메라·가시성 계산을 제공한다.
#include "EnginePCH.h"
#include "Editor/LevelEditor/MultipleViewports/Core/MultipleViewports.h"

#include <algorithm>
#include <cassert>
#include <math.h>
#include "Camera/ViewMath.h"
#include "Math/EngineMath.h"

// MultipleViewportsMax의 비교 순서와 동률 선택을 보존하는 float 전용 보조 함수다.
static float MultipleViewportsMax(float A, float B) { return A < B ? B : A; }
// MultipleViewportsMin의 비교 순서와 동률 선택을 보존하는 float 전용 보조 함수다.
static float MultipleViewportsMin(float A, float B) { return B < A ? B : A; }
// 두 float을 임시 값 하나로 교환한다.
static void MultipleViewportsSwap(float& A, float& B) { const float Temp = A; A = B; B = Temp; }

// 세 축의 slab 구간을 교차해 Ray와 AABB의 충돌을 검사한다.
static bool MultipleViewportsRayIntersectsAABB(const FRay& Ray, const FAABB& Bounds)
{
    const float Origins[3] = {Ray.Origin.X, Ray.Origin.Y, Ray.Origin.Z};
    const float Directions[3] = {Ray.Direction.X, Ray.Direction.Y, Ray.Direction.Z};
    const float Centers[3] = {Bounds.Center.X, Bounds.Center.Y, Bounds.Center.Z};
    const float Extents[3] = {Bounds.Extent.X, Bounds.Extent.Y, Bounds.Extent.Z};
    float Minimum = 0.0f;
    float Maximum = HUGE_VALF;

    for (int Axis = 0; Axis < 3; ++Axis)
    {
        const float Low = Centers[Axis] - Extents[Axis];
        const float High = Centers[Axis] + Extents[Axis];
        if (fabsf(Directions[Axis]) <= ViewMath::Epsilon)
        {
            if (Origins[Axis] < Low || Origins[Axis] > High)
            {
                return false;
            }
            continue;
        }

        float Near = (Low - Origins[Axis]) / Directions[Axis];
        float Far = (High - Origins[Axis]) / Directions[Axis];
        if (Near > Far)
        {
            MultipleViewportsSwap(Near, Far);
        }
        Minimum = MultipleViewportsMax(Minimum, Near);
        Maximum = MultipleViewportsMin(Maximum, Far);
        if (Minimum > Maximum)
        {
            return false;
        }
    }
    return Maximum >= 0.0f;
}

// Möller–Trumbore 알고리즘으로 Ray와 삼각형의 교차 거리를 구한다.
static bool MultipleViewportsRayIntersectsTriangle(const FRay& Ray, const FTriangle& Triangle, float& OutDistance)
{
    const FVector Edge1 = ViewMath::Subtract(Triangle.V1, Triangle.V0);
    const FVector Edge2 = ViewMath::Subtract(Triangle.V2, Triangle.V0);
    const FVector P = ViewMath::Cross(Ray.Direction, Edge2);
    const float Determinant = ViewMath::Dot(Edge1, P);
    if (fabsf(Determinant) <= ViewMath::Epsilon)
    {
        return false;
    }

    const float InverseDeterminant = 1.0f / Determinant;
    const FVector T = ViewMath::Subtract(Ray.Origin, Triangle.V0);
    const float U = ViewMath::Dot(T, P) * InverseDeterminant;
    if (U < 0.0f || U > 1.0f)
    {
        return false;
    }

    const FVector Q = ViewMath::Cross(T, Edge1);
    const float V = ViewMath::Dot(Ray.Direction, Q) * InverseDeterminant;
    if (V < 0.0f || U + V > 1.0f)
    {
        return false;
    }

    const float Distance = ViewMath::Dot(Edge2, Q) * InverseDeterminant;
    if (Distance < 0.0f)
    {
        return false;
    }
    OutDistance = Distance;
    return true;
}

// Rect가 렌더 가능한 양수 크기인지 확인한다.
bool IsViewRectValid(const FRect& Rect) { return Rect.Width > 0.0f && Rect.Height > 0.0f; }

// Rect 유효성과 Single·Quad 레이아웃 규칙으로 View 활성 여부를 판정한다.
bool IsViewActive(const FViewSet& Views, const int32 ViewIndex, const FRect ViewRects[4])
{
    if (ViewIndex < 0 || ViewIndex >= 4 || !IsViewRectValid(ViewRects[ViewIndex]))
    {
        return false;
    }
    return Views.Mode == ELayoutMode::QuadSplit || ViewIndex == 0;
}

// 네 Rect를 순서대로 검사해 화면 좌표를 포함하는 View를 찾는다.
int32 DetermineHoveredView(const FVector2 ScreenPos, const FRect ViewRects[4])
{
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const FRect& Rect = ViewRects[Index];
        if (IsViewRectValid(Rect) && ScreenPos.X >= Rect.X && ScreenPos.X < Rect.X + Rect.Width && ScreenPos.Y >= Rect.Y && ScreenPos.Y < Rect.Y + Rect.Height)
        {
            return Index;
        }
    }
    return InvalidViewIndex;
}

// Capture 중에는 고정 View를 유지하고 아니면 Hover View를 사용한다.
int32 DetermineActiveView(const FViewInputState& State, const FVector2 ScreenPos, const FRect ViewRects[4])
{
    if (State.CapturedViewIndex != InvalidViewIndex)
    {
        return State.CapturedViewIndex;
    }
    return DetermineHoveredView(ScreenPos, ViewRects);
}

// 기존 입력 상태를 복사한 뒤 지정 View에 Capture를 시작한다.
FViewInputState BeginCapture(const FViewInputState& Current, const int32 ViewIndex)
{
    assert(ViewIndex >= 0 && ViewIndex < 4);
    FViewInputState Result = Current;
    Result.CapturedViewIndex = ViewIndex;
    return Result;
}

// 기존 입력 상태를 복사한 뒤 Capture View를 해제한다.
FViewInputState EndCapture(const FViewInputState& Current)
{
    FViewInputState Result = Current;
    Result.CapturedViewIndex = InvalidViewIndex;
    return Result;
}

// Drag 픽셀을 창 축 길이로 나눠 Split 비율에 누적하고 clamp한다.
FSplitRatio ApplySplitterDrag(const FSplitRatio& Current, const EDragAxis Axis, const float DeltaPixels, const FVector2 WindowSize, const float MinRatio)
{
    FSplitRatio Result = Current;
    if (Axis == EDragAxis::Horizontal)
    {
        assert(WindowSize.X > 0.0f);
        Result.Horizontal += DeltaPixels / WindowSize.X;
    }
    else
    {
        assert(WindowSize.Y > 0.0f);
        Result.Vertical += DeltaPixels / WindowSize.Y;
    }
    return ClampSplitRatio(Result, MinRatio);
}

// Splitter가 창 가장자리에 붙지 않도록 두 비율을 대칭 범위로 제한한다.
FSplitRatio ClampSplitRatio(const FSplitRatio& Raw, const float MinRatio)
{
    assert(MinRatio >= 0.0f && MinRatio <= 0.5f);
    return {
        FMath::Clamp(Raw.Horizontal, MinRatio, 1.0f - MinRatio),
        FMath::Clamp(Raw.Vertical, MinRatio, 1.0f - MinRatio)};
}

// 가로·세로 Split 위치로 창을 빈틈없는 네 Rect로 나눈다.
void ComputeViewRects(const FSplitRatio& Ratio, const FVector2 WindowSize, FRect OutRects[4])
{
    assert(WindowSize.X >= 0.0f && WindowSize.Y >= 0.0f);
    assert(Ratio.Horizontal >= 0.0f && Ratio.Horizontal <= 1.0f);
    assert(Ratio.Vertical >= 0.0f && Ratio.Vertical <= 1.0f);
    const float LeftWidth = WindowSize.X * Ratio.Horizontal;
    const float TopHeight = WindowSize.Y * Ratio.Vertical;
    const float RightWidth = WindowSize.X - LeftWidth;
    const float BottomHeight = WindowSize.Y - TopHeight;
    OutRects[0] = {0.0f, 0.0f, LeftWidth, TopHeight};
    OutRects[1] = {LeftWidth, 0.0f, RightWidth, TopHeight};
    OutRects[2] = {0.0f, TopHeight, LeftWidth, BottomHeight};
    OutRects[3] = {LeftWidth, TopHeight, RightWidth, BottomHeight};
}

// 절두체 검사에 통과한 렌더 대상 ID를 재사용 출력 버퍼에 모은다.
void CullForView(const TArray<FRenderableObject>& WorldObjects, const FFrustumPlanes& Frustum, TArray<ObjectId>& OutVisibleIds)
{
    OutVisibleIds.Reset();
    for (const FRenderableObject& Object : WorldObjects)
    {
        if (IsAABBInFrustum(Object.WorldBounds, Frustum))
        {
            OutVisibleIds.Add(Object.Id);
        }
    }
}

// Ray와 월드 AABB가 교차하는 대상만 Narrow Phase 후보로 모은다.
void FindPickCandidates(const FRay& WorldRay, const TArray<FPickableObject>& Objects, TArray<ObjectId>& OutCandidates)
{
    OutCandidates.Reset();
    for (const FPickableObject& Object : Objects)
    {
        if (MultipleViewportsRayIntersectsAABB(WorldRay, Object.WorldBounds))
        {
            OutCandidates.Add(Object.Id);
        }
    }
}

// 후보별 모든 삼각형을 검사해 Ray에 가장 가까운 양의 교차를 선택한다.
FPickHit PickNarrowPhase(const FRay& WorldRay, const TArray<ObjectId>& Candidates, const TMap<ObjectId, TArray<FTriangle>>& TrianglesById)
{
    FPickHit Result{};
    float Nearest = HUGE_VALF;
    for (const ObjectId Id : Candidates)
    {
        const auto* Found = TrianglesById.Find(Id);
        if (Found == nullptr)
        {
            continue;
        }
        for (const FTriangle& Triangle : *Found)
        {
            float Distance = 0.0f;
            if (MultipleViewportsRayIntersectsTriangle(WorldRay, Triangle, Distance) && Distance < Nearest)
            {
                Nearest = Distance;
                Result.bHit = true;
                Result.Id = Id;
                Result.Distance = Distance;
                Result.HitPoint = ViewMath::Add(WorldRay.Origin, ViewMath::Scale(WorldRay.Direction, Distance));
            }
        }
    }
    return Result;
}

// AABB Broad Phase와 삼각형 Narrow Phase를 차례로 수행한다.
FPickHit Pick(const FRay& WorldRay, const TArray<FPickableObject>& Objects, const TMap<ObjectId, TArray<FTriangle>>& TrianglesById)
{
    TArray<ObjectId> Candidates;
    FindPickCandidates(WorldRay, Objects, Candidates);
    if (Candidates.IsEmpty())
    {
        return {};
    }
    return PickNarrowPhase(WorldRay, Candidates, TrianglesById);
}

// 거리 제곱을 캐시한 뒤 stable sort로 먼 파티클부터 ID를 출력한다.
void SortParticlesByCameraDistance(const TArray<FParticleSortInput>& Particles, const FVector& CameraLocation, TArray<ObjectId>& OutSortedBackToFront)
{
    // 파티클 ID와 미리 계산한 카메라 거리 제곱을 함께 담는다.
    struct FEntry { ObjectId Id; float DistanceSquared; };
    TArray<FEntry> Entries;
    for (const FParticleSortInput& Particle : Particles)
    {
        Entries.Add({Particle.Id, ViewMath::LengthSquared(ViewMath::Subtract(Particle.WorldPosition, CameraLocation))});
    }
    std::stable_sort(Entries.begin(), Entries.end(), [](const FEntry& A, const FEntry& B) { return A.DistanceSquared > B.DistanceSquared; });
    OutSortedBackToFront.Reset();
    for (const FEntry& Entry : Entries)
    {
        OutSortedBackToFront.Add(Entry.Id);
    }
}
