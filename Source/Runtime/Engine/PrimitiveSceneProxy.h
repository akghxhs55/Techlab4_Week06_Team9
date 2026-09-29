#pragma once

#include "Math/Frustum.h"

class FScene;
class UPrimitiveComponent;

class FPrimitiveSceneProxy
{
public:
	explicit FPrimitiveSceneProxy(UPrimitiveComponent* InComponent) :Component(InComponent) {}

	void UpdateTransform();

	UPrimitiveComponent* GetComponent() const { return Component; }
	const FAABB GetBounds() const { return Bounds; }
	const FMatrix& GetLocalToWorld() const { return LocalToWorld; }

	FScene* GetScene() const { return Scene; }
private:
	friend class FScene;
	FScene* Scene = nullptr;

	UPrimitiveComponent* Component = nullptr;
	FMatrix LocalToWorld;
	FAABB Bounds;
	int32 PackedIndex = INDEX_NONE;

	bool bQueuedForUpdate = false;
};
