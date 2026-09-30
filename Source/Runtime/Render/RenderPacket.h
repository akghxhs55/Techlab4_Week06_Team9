#pragma once
#include "Math/Transform.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"

#include <vector>

// Todo: subuv
class UTexture2D;
class FPrimitiveSceneProxy;

inline constexpr uint32 InvalidObjectSlot = ~0u;

struct FRenderPacket
{
	const FPrimitiveSceneProxy* Proxy = nullptr;

	FMatrix model;
	UStaticMesh* Mesh = nullptr;
	UMaterial* Material = nullptr;
	uint8 LODIndex = 0;
	// GPU 오클루전이 가렸다고 판정한 물체. Cull을 끈 검증 모드에서만 이 값이 true인 패킷이 그려진다.
	bool bOccludedByGpu = false;

	// 카메라와의 거리 제곱. 반투명 정렬에 사용
	float CameraToParticleDistance = 0.0f;

	const void* MaterialParamData = nullptr;
	uint32 MaterialParamDataSize = 0;

	uint32 StartIndex = 0;
	uint32 IndexCount = 0; // 0이면 전체 IndexBuffer 사용

	uint32 Slot = InvalidObjectSlot;
};

// 캐시된 스태틱 메시의 불투명 드로우 하나. Gather가 병렬로 만들고 Renderer가 그대로 그린다.
// 머티리얼·메시·LOD는 묶음(FStaticDrawGroup)이 들고 있으므로 항목에는 물체마다 다른 값만 둔다.
struct FStaticDrawItem
{
	const FPrimitiveSceneProxy* Proxy;   // 측정 도구가 물체 단위로 세는 데와 칸이 없을 때 행렬을 읽는 데 쓴다
	uint32 Slot;                         // 물체별 상수 버퍼 칸 (InvalidObjectSlot이면 PerObjectCB로 갱신)
	uint32 StartIndex;
	uint32 IndexCount;
	uint32 bOccludedByGpu;               // GPU 오클루전 검증 모드에서 가렸다고 판정한 물체
};

// 머티리얼·메시·LOD가 같은 드로우 묶음. Gather 조각마다 따로 가지므로 락 없이 채운다.
// 바인딩은 묶음마다 한 번이고, 정렬은 항목이 아니라 묶음(수십 개)만 한다.
struct FStaticDrawGroup
{
	UMaterial* Material = nullptr;
	UStaticMesh* Mesh = nullptr;
	uint8 LODIndex = 0;
	std::vector<FStaticDrawItem> Items;   // 매 프레임 비우고 용량은 재사용
};