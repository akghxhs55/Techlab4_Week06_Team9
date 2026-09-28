#pragma once

#include "Math/Matrix.h"
#include "Core/Types.h"

class UStaticMesh;

struct FLODViewContext
{
    FMatrix ViewProjection;
    uint32 Width = 0;
    uint32 Height = 0;
};

uint32 SelectStaticMeshLOD(const UStaticMesh& Mesh,const FMatrix& WorldMatrix, const FLODViewContext& View);