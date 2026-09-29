#pragma once

#include "Math/Matrix.h"
#include "Math/Frustum.h"
#include "Math/Vector.h"
#include "Core/Types.h"

class UStaticMesh;

struct FLODViewContext
{
    FMatrix ViewProjection;
    uint32 Width = 0;
    uint32 Height = 0;

    FVector CameraPosition;
    FVector CameraForward;       // 정규화된 월드 전방
    float ProjectionScaleSquared = 0.0f;
    float NearZ = 0.0f;
    bool bOrthographic = false;
};

uint32 SelectStaticMeshLOD(const UStaticMesh& Mesh,const FAABB& WorldBounds,const FLODViewContext& View);