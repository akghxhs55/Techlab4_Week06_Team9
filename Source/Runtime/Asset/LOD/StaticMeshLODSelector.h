#pragma once

#include "Camera/RenderView.h"
#include "Core/Types.h"
#include "Engine/PrimitiveSceneProxy.h"

struct FLODViewContext
{
    const FRenderView& View;
	float ProjectionScaleSquared = 0.0f;
    float CameraDepth = 0.0f;

	explicit FLODViewContext(const FRenderView& InView)
		: View(InView)
	{
		CameraDepth = InView.CameraLocation.Dot(InView.CameraForward);
		ProjectionScaleSquared = std::max(InView.Projection.M[1][0] * InView.Projection.M[1][0], InView.Projection.M[2][1] * InView.Projection.M[2][1]);
	}
};

template<bool Orthographic>
inline uint32 SelectSphereLOD(const FLODSelectionInput& Input, const FLODViewContext& Context)
{
    const FMeshRenderState* State = Input.State;
    if (!State || State->LODCount <= 1 || Context.View.ViewSize.X == 0 || Context.View.ViewSize.Y == 0) return 0;
    const FLODSphere& Sphere = Input.Sphere;
    const float Depth = Sphere.Center.X * Context.View.CameraForward.X
        + Sphere.Center.Y * Context.View.CameraForward.Y + Sphere.Center.Z * Context.View.CameraForward.Z - Context.CameraDepth;
    const float NearDistance = Depth - Context.View.NearZ;
    if (NearDistance <= 0.0f || NearDistance * NearDistance <= Sphere.RadiusSquared) return 0;
    const float Numerator = Sphere.RadiusSquared * Context.ProjectionScaleSquared;
    const float DistanceFactor = Orthographic ? 1.0f : Depth * Depth;
    uint32 DesiredLOD = 3;
    if (Numerator >= State->LODThresholdSq[0] * DistanceFactor) DesiredLOD = 0;
    else if (Numerator >= State->LODThresholdSq[1] * DistanceFactor) DesiredLOD = 1;
    else if (Numerator >= State->LODThresholdSq[2] * DistanceFactor) DesiredLOD = 2;
    return DesiredLOD < State->LODCount ? DesiredLOD : State->LODCount - 1;
}

inline uint32 SelectLOD(const FPrimitiveSceneProxy& Proxy, const FLODViewContext& Context)
{
    const FLODSelectionInput Input{Proxy.GetLODSphere(), Proxy.GetRenderState()};
    return Context.View.bIsOrthogonal ? SelectSphereLOD<true>(Input, Context) : SelectSphereLOD<false>(Input, Context);
}

// Recomputed for every current view; previous selections are never reused.
void SelectLODs(const TArray<FLODSelectionInput>& Inputs, const FLODViewContext& Context, TArray<uint8>& OutLODs);
