#pragma once

#include "Math/Matrix.h"
#include "Math/Frustum.h"
#include "Math/Vector.h"
#include "Core/Types.h"
#include "Engine/PrimitiveSceneProxy.h"

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

// SelectStaticMeshLOD와 같은 판정을 프록시에 캐싱된 값(바운드·LOD 수·임계값 제곱)만으로 한다.
// Gather 루프에서 물체마다 불리므로 헤더에 인라인으로 두어 호출 비용과 메시 역참조를 없앤다.
uint32 SelectLOD(const FPrimitiveSceneProxy& Proxy, const FLODViewContext& View);
