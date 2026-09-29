#include "EnginePCH.h"
#include "StaticMeshLODSelector.h"

#include "Render/Mesh.h"

#include <algorithm>
#include <array>
#include <cfloat>

namespace
{
    std::array<FVector, 8> MakeBoxCorners(const FBox& Box)
    {
        return {{
            {Box.Min.X, Box.Min.Y, Box.Min.Z},
            {Box.Max.X, Box.Min.Y, Box.Min.Z},
            {Box.Min.X, Box.Max.Y, Box.Min.Z},
            {Box.Max.X, Box.Max.Y, Box.Min.Z},
            {Box.Min.X, Box.Min.Y, Box.Max.Z},
            {Box.Max.X, Box.Min.Y, Box.Max.Z},
            {Box.Min.X, Box.Max.Y, Box.Max.Z},
            {Box.Max.X, Box.Max.Y, Box.Max.Z},
        }};
    }
}

uint32 SelectStaticMeshLOD(const UStaticMesh& Mesh,const FAABB& Bounds, const FLODViewContext& View)
{
    const uint32 LODCount = Mesh.GetLODCount();
    if (LODCount <= 1 || View.Width == 0 || View.Height == 0) return 0;

    const float RadiusSquared = Bounds.Extent.Dot(Bounds.Extent);
    const float Depth = (Bounds.Center - View.CameraPosition).Dot(View.CameraForward);

    // 구가 근평면에 걸리면 기존 방식처럼 보수적으로 LOD0.
    const float NearDistance = Depth - View.NearZ;
    if (NearDistance <= 0.0f || NearDistance * NearDistance <= RadiusSquared) return 0;

    const float SizeNumerator = RadiusSquared * View.ProjectionScaleSquared;

    // 직교 투영에서는 거리에 따라 화면 크기가 변하지 않는다.
    const float DistanceFactor = View.bOrthographic ? 1.0f : Depth * Depth;

    uint32 DesiredLOD = 3;
    if (SizeNumerator >= Mesh.ScreenThresholds[0] * Mesh.ScreenThresholds[0] * DistanceFactor)
        DesiredLOD = 0;
    else if (SizeNumerator >= Mesh.ScreenThresholds[1] * Mesh.ScreenThresholds[1] * DistanceFactor)
        DesiredLOD = 1;
    else if (SizeNumerator >= Mesh.ScreenThresholds[2] * Mesh.ScreenThresholds[2] * DistanceFactor)
        DesiredLOD = 2;

    return std::min(DesiredLOD, LODCount - 1);
}
