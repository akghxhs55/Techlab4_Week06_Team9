#pragma once

#include "Box.h"

struct FPlane { FVector Normal; float Distance; };
struct FFrustumPlanes { FPlane Planes[6]; };
struct FAABB { FVector Center; FVector Extent; };

static FPlane NormalizePlane(const FPlane Plane);
FFrustumPlanes ExtractFrustumPlanes(const FMatrix& Matrix);
bool IsAABBInFrustum(const FAABB& Bounds, const FFrustumPlanes& Frustum);
FAABB MakeWorldBounds(const FBox& Value);