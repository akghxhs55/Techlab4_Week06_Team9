#include "EnginePCH.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

#include "Core/EngineTimer.h"
#include "Core/Stats/LightweightStats.h"

#include "RenderCommand.h"

#include "Camera/CameraComponent.h"

#include <algorithm>

DECLARE_CYCLE_STAT("Draw Render Packets", STAT_DrawRenderPackets);
DECLARE_CYCLE_STAT("Render Queue Sorting", STAT_RenderQueueSorting);
DECLARE_CYCLE_STAT("Upload Per-Object CB", STAT_UploadPerObjectCB);

namespace
{
	// VSSetConstantBuffers1의 오프셋은 16개 상수(256바이트) 단위여야 하므로 오브젝트마다 256바이트 칸을 쓴다.
	constexpr uint32 PerObjectSlotConstants = 16;
	constexpr uint32 PerObjectSlotBytes = PerObjectSlotConstants * 16;
	static_assert(sizeof(FPerObjectConstants) <= PerObjectSlotBytes);

	constexpr uint32 MinPerObjectSlots = 1024;
}

bool FRenderer::Init()
{
	bUsePerObjectSlots = RenderCommand::SupportsConstantBufferOffsets();
	PerObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FPerObjectConstants));
	ViewCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));

	return true;
}

// 필요한 칸 수가 용량을 넘을 때만 두 배씩 키워 재할당을 드물게 한다.
void FRenderer::EnsurePerObjectSlotCapacity(uint32 SlotCount)
{
	if (SlotCount <= PerObjectSlotCapacity)
		return;

	uint32 NewCapacity = std::max(PerObjectSlotCapacity * 2, MinPerObjectSlots);
	while (NewCapacity < SlotCount)
		NewCapacity *= 2;

	PerObjectSlotCB = RenderCommand::CreateConstantBuffer(NewCapacity * PerObjectSlotBytes);
	if (!PerObjectSlotCB || !PerObjectSlotCB->GetBuffer())
	{
		HTR_LOG(Warning, "Per-object constant buffer ({} slots) creation failed. Falling back to per-draw updates.", NewCapacity);
		PerObjectSlotCB.reset();
		PerObjectSlotCapacity = 0;
		return;
	}
	PerObjectSlotCapacity = NewCapacity;
}

// 정렬된 순서대로 모든 패킷의 World 행렬을 한 번의 Map으로 올린다. 패킷 i는 칸 i를 쓴다.
void FRenderer::UploadPerObjectConstants()
{
	SCOPE_CYCLE_COUNTER(STAT_UploadPerObjectCB);

	const uint32 Count = static_cast<uint32>(RenderPackets.size());
	if (!bUsePerObjectSlots || Count == 0)
		return;

	EnsurePerObjectSlotCapacity(Count);
	if (!PerObjectSlotCB)
	{
		bUsePerObjectSlots = false;
		return;
	}

	uint8* Dest = static_cast<uint8*>(RenderCommand::MapWriteDiscard(PerObjectSlotCB.get()));
	if (!Dest)
	{
		bUsePerObjectSlots = false;
		return;
	}

	// 매핑된 메모리는 write-combined라 순차 쓰기만 하고 읽지 않는다.
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const FMatrix World = RenderPackets[Index].model.GetTransposed();
		std::memcpy(Dest + static_cast<size_t>(Index) * PerObjectSlotBytes, &World, sizeof(FMatrix));
	}

	RenderCommand::Unmap(PerObjectSlotCB.get());
}

// 카메라의 ViewProjection을 공통 렌더 경로로 전달한다.
void FRenderer::RenderAll(TArray<FRenderPacket>& InQueue, UCameraComponent* CameraComponent)
{
	RenderAll(InQueue, CameraComponent->GetViewProjectionMatrix());
}

// 불투명 우선·반투명 거리순으로 정렬해 View 행렬과 Section 범위로 그린다.
void FRenderer::RenderAll(TArray<FRenderPacket>& InQueue, const FMatrix& ViewProjection)
{
	RenderQueueSorting(InQueue, ViewProjection);
	RenderOpaque(ViewProjection);
	RenderTranslucent(ViewProjection);
}

void FRenderer::RenderOpaque(const FMatrix& ViewProjection)
{
	DrawPackets(0, FirstTranslucentIndex, ViewProjection);
}

// RenderOpaque가 남긴 반투명 패킷을 먼 것부터 그린다.
void FRenderer::RenderTranslucent(const FMatrix& ViewProjection)
{
	DrawPackets(FirstTranslucentIndex, static_cast<uint32>(RenderPackets.size()), ViewProjection);
	RenderPackets.Reset();
	FirstTranslucentIndex = 0;
}

void FRenderer::RenderQueueSorting(TArray<FRenderPacket>& InQueue, const FMatrix& ViewProjection)
{
	FMatrix VP = ViewProjection.GetTransposed();
	RenderCommand::UpdateBufferData(ViewCB.get(), &VP);
	{
	SCOPE_CYCLE_COUNTER(STAT_RenderQueueSorting);

	std::swap(RenderPackets, InQueue);
	InQueue.Reset();

	std::sort(
		RenderPackets.begin(),
		RenderPackets.end(),
		[](const FRenderPacket& First, const FRenderPacket& Second) -> bool
		{
			const bool bFirstTranslucent = First.material->BlendState != EBlendState::Opaque;
			const bool bSecondTranslucent = Second.material->BlendState != EBlendState::Opaque;

			if (bFirstTranslucent != bSecondTranslucent) { return !bFirstTranslucent; }
			if (bFirstTranslucent) { return First.CameraToParticleDistance > Second.CameraToParticleDistance; }
			if (First.material != Second.material) { return std::less<UMaterial*>{}(First.material, Second.material); }
			if (First.mesh != Second.mesh) { return std::less<UStaticMesh*>{}(First.mesh, Second.mesh); }
			return std::less<uint8>{}(First.LODIndex, Second.LODIndex);
		});

	// 정렬 결과 반투명은 뒤쪽에 모이므로 첫 반투명 위치에서 두 패스를 나눈다.
	FirstTranslucentIndex = 0;
	while (FirstTranslucentIndex < RenderPackets.size()
		&& (RenderPackets[FirstTranslucentIndex].material == nullptr
			|| RenderPackets[FirstTranslucentIndex].material->BlendState == EBlendState::Opaque))
	{
		++FirstTranslucentIndex;
	}
	}

	UploadPerObjectConstants();
}

// 정렬된 패킷 중 [Begin, End) 범위를 View 행렬과 Section 범위로 그린다.
void FRenderer::DrawPackets(uint32 Begin, uint32 End, const FMatrix& ViewProjection)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);

	LastMesh = nullptr;
	LastMaterial = nullptr;
	uint8 LastLODIndex = 0;

	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex);
	if (!bUsePerObjectSlots)
		RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);

	for (uint32 Index = Begin; Index < End; ++Index)
	{
		const FRenderPacket& RenderPacket = RenderPackets[Index];
		if (RenderPacket.mesh == nullptr || RenderPacket.material == nullptr) continue;
		if (RenderPacket.mesh != LastMesh || RenderPacket.LODIndex != LastLODIndex) {
			RenderCommand::BindMesh(RenderPacket.mesh,RenderPacket.LODIndex);
		}
		if (RenderPacket.material != LastMaterial) {
			BindMaterial(RenderPacket.material);
			UpdateMaterialParams(RenderPacket);
		}
		if (bUsePerObjectSlots)
		{
			// 드로우마다 Map하지 않고 이미 올린 칸의 오프셋만 바꾼다.
			RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(),
				Index * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex);
		}
		else
		{
			UpdatePerObjectConstants(RenderPacket, ViewProjection);
		}

		RenderCommand::DrawIndexed(
			RenderPacket.IndexCount ? RenderPacket.IndexCount : RenderPacket.mesh->GetIndexBuffer(RenderPacket.LODIndex)->GetIndexCount(),
			RenderPacket.StartIndex
		);
		LastMaterial = RenderPacket.material;
		LastMesh = RenderPacket.mesh;
		LastLODIndex = RenderPacket.LODIndex;
	}
}

// Material마다 Shader/Texture/Sampler/State 꽂기
void FRenderer::BindMaterial(UMaterial* material)
{
	RenderCommand::BindShaderProgram(material->Shader);
	RenderCommand::SetBlendState(material->BlendState);
	// 반투명은 뒤에 그려지는 Grid·다른 반투명을 가리지 않도록 깊이를 쓰지 않는다.
	const bool bTranslucent = material->BlendState != EBlendState::Opaque;
	RenderCommand::SetDepthStencilState(bTranslucent && material->DepthStencilState == EDepthStencilState::Default
		? EDepthStencilState::ReadOnly : material->DepthStencilState);

	for (int i = 0; i < material->Textures.size(); i++)
	{
		RenderCommand::BindShaderResource(i, material->Textures[i], EShaderBindFlagBits::Pixel);
	}
	RenderCommand::BindSamplerState(0, material->SamplerState, EShaderBindFlagBits::Pixel);
}

// b1 내용 채우고 꽂기
void FRenderer::UpdateMaterialParams(const FRenderPacket& RenderPacket)
{
	switch (RenderPacket.material->ParamLayout)
	{
		case EMaterialParamLayout::StaticMesh:
		{
			break; // 라이팅 적용 시 제거

			const float TotalTime = EngineTimer::GetTotalTime();

			FStaticMeshMaterialParams Params{};
			Params.BaseColor = RenderPacket.material->BaseColor;
			Params.UVOffset = RenderPacket.material->UVScrollSpeed * TotalTime;
			Params.bOpaque = RenderPacket.material->BlendState == EBlendState::Opaque ? 1.0f : 0.0f;

			RenderCommand::UpdateBufferData(RenderPacket.material->ParamBuffer.get(), &Params, sizeof(FStaticMeshMaterialParams));
			RenderCommand::BindConstantBuffer(1, RenderPacket.material->ParamBuffer.get(), EShaderBindFlagBits::Pixel);
			break;
		}
		case EMaterialParamLayout::ParticleSubUV:
		{
			if (RenderPacket.material->ParamBuffer && RenderPacket.MaterialParamData != nullptr)
			{
				RenderCommand::UpdateBufferData(RenderPacket.material->ParamBuffer.get(), RenderPacket.MaterialParamData, RenderPacket.MaterialParamDataSize);
				RenderCommand::BindConstantBuffer(1, RenderPacket.material->ParamBuffer.get(), EShaderBindFlagBits::Pixel);
			}
			break;
		}
		case EMaterialParamLayout::None:
		{
			break;
		}
	}
}

// b0 MVP 채우고 꽂기
void FRenderer::UpdatePerObjectConstants(const FRenderPacket& RenderPacket, const FMatrix& ViewProjection)
{
	// rp.Transform 과 Camera VP 행렬 곱
	// 행렬곱의 결과 (MVP Matrix) Constant Buffer 업데이트 필요

	FPerObjectConstants Constants;

	Constants.World = RenderPacket.model.GetTransposed();

	RenderCommand::UpdateBufferData(PerObjectCB.get(), &Constants);
}
