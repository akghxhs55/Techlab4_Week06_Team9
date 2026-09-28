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

uint32 SelectStaticMeshLOD(const UStaticMesh& Mesh, const FMatrix& WorldMatrix, const FLODViewContext& View)
{
    if (View.Width == 0 || View.Height == 0)
        return 0;

    const FBox& LocalBox = Mesh.GetMeshData().AABB;
    const FMatrix LocalToClip = WorldMatrix * View.ViewProjection;

    float MinX = FLT_MAX, MinY = FLT_MAX;
    float MaxX = -FLT_MAX, MaxY = -FLT_MAX;

    for (const FVector& Corner : MakeBoxCorners(LocalBox))
    {
        const FVector4 Clip = FVector4(Corner, 1.0f) * LocalToClip;

        // Near plane과 겹치는 큰 물체는 보수적으로 LOD0.
        if (Clip.W <= 1e-5f || Clip.Z < 0.0f)
            return 0;

        const float X = (Clip.X / Clip.W * 0.5f + 0.5f) * View.Width;
        const float Y = (1.0f - (Clip.Y / Clip.W * 0.5f + 0.5f)) * View.Height;

        MinX = std::min(MinX, X);
        MinY = std::min(MinY, Y);
        MaxX = std::max(MaxX, X);
        MaxY = std::max(MaxY, Y);
    }

    const float ScreenSize = std::max(
        (MaxX - MinX) / static_cast<float>(View.Width),
        (MaxY - MinY) / static_cast<float>(View.Height));

    uint32 DesiredLOD = 3;
    if (ScreenSize >= Mesh.ScreenThresholds[0]) DesiredLOD = 0;
    else if (ScreenSize >= Mesh.ScreenThresholds[1]) DesiredLOD = 1;
    else if (ScreenSize >= Mesh.ScreenThresholds[2]) DesiredLOD = 2;

    return std::min(DesiredLOD, Mesh.GetLODCount() - 1);
}
