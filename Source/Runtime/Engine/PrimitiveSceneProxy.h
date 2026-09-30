#pragma once

#include "Math/Frustum.h"

class FScene;
class UPrimitiveComponent;
class UStaticMesh;
class UMaterial;

struct FCachedMeshSection
{
	UMaterial* Material;
	uint32 StartIndex;
	uint32 IndexCount;
};

struct FCachedMeshLOD
{
	uint32 FirstSection; // Sections 내부 시작 인덱스
	uint32 NumSections; // LODSection 개수
};

class FPrimitiveSceneProxy
{
public:
	explicit FPrimitiveSceneProxy(UPrimitiveComponent* InComponent) :Component(InComponent) {}

	void UpdateTransform();
	void UpdateRenderState();

	UPrimitiveComponent* GetComponent() const { return Component; }
	const FAABB& GetBounds() const { return Bounds; }
	const FMatrix& GetLocalToWorld() const { return LocalToWorld; }
	const FMatrix& GetWorldToLocal() const { return WorldToLocal; }

	UStaticMesh* GetMesh() const { return Mesh; }
	bool IsVisible() const { return bVisible; }
	uint8 GetLODCount() const { return LODCount; }
	const float* GetLODThresholdsSq() const { return LODThresholdSq; }
	const FCachedMeshLOD& GetLOD(uint32 Index) const { return LODs[Index]; }
	const FCachedMeshSection& GetSection(uint32 Index) const { return Sections[Index]; }

	FScene* GetScene() const { return Scene; }
private:
	friend class FScene;
	FScene* Scene = nullptr;

	UPrimitiveComponent* Component = nullptr;
	FMatrix LocalToWorld;
	FMatrix WorldToLocal;
	FAABB Bounds;
	int32 PackedIndex = INDEX_NONE;

	bool bQueuedForUpdate = false;
	bool bRenderStateQueued = false;

	UStaticMesh* Mesh = nullptr;
	bool bVisible = true;
	uint8 LODCount = 1;
	float LODThresholdSq[3];
	FCachedMeshLOD LODs[4];
	TArray<FCachedMeshSection> Sections;
};
