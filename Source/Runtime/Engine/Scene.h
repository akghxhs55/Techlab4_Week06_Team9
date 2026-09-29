#pragma once

#include "PrimitiveSceneProxy.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Math/Frustum.h"
#include "Math/BVH.h"

class FScene
{
public:
	void AddPrimitive(UPrimitiveComponent* Component);
	void RemovePrimitive(UPrimitiveComponent* Component);

	void UpdateAllTransforms();

	void BuildBVH();

	void MarkDirty(FPrimitiveSceneProxy* Proxy);
	

	TArray<FPrimitiveSceneProxy*> Proxies;
	TArray<FPrimitiveSceneProxy*> DirtyProxies;
	TArray<FAABB> PrimitiveBounds;
	TArray<uint8> PrimitiveFlags;

	// 컬링 결과로 프록시를 바로 내보내 컴포넌트를 역참조하지 않는다. 경계는 Build/Refit 때만 계산한다.
	TBVH<FPrimitiveSceneProxy*> BVH{
		[](const FPrimitiveSceneProxy* Proxy) -> FBox
		{
			return Proxy->GetComponent()->CalcBounds();
		}
	};
	bool bElementListChanged = false;

	
};