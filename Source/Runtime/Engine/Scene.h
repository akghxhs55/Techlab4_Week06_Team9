#pragma once

#include "PrimitiveSceneProxy.h"
#include "Component/PrimitiveComponent.h"
#include "Math/Frustum.h"

class FScene
{
public:
	void AddPrimitive(UPrimitiveComponent* Component);
	void RemovePrimitive(UPrimitiveComponent* Component);

	void UpdateAllTransforms();

	TArray<FPrimitiveSceneProxy*> Proxies;
	TArray<FAABB> PrimitiveBounds;
	TArray<uint8> PrimitiveFlags;
};