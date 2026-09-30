#include "EnginePCH.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

#include "Core/EngineTimer.h"
#include "Core/Stats/LightweightStats.h"

#include "Engine/PrimitiveSceneProxy.h"

#include "RenderCommand.h"

#include "Camera/CameraComponent.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>

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
		const FRenderPacket& P = RenderPackets[Index];
		const FMatrix& Model = P.Proxy ? P.Proxy->GetLocalToWorld() : P.model;
		const FMatrix World = Model.GetTransposed();
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
			const bool bFirstTranslucent = First.Material->BlendState != EBlendState::Opaque;
			const bool bSecondTranslucent = Second.Material->BlendState != EBlendState::Opaque;

			if (bFirstTranslucent != bSecondTranslucent) { return !bFirstTranslucent; }
			if (bFirstTranslucent) { return First.CameraToParticleDistance > Second.CameraToParticleDistance; }
			if (First.Material != Second.Material) { return std::less<UMaterial*>{}(First.Material, Second.Material); }
			if (First.Mesh != Second.Mesh) { return std::less<UStaticMesh*>{}(First.Mesh, Second.Mesh); }
			return std::less<uint8>{}(First.LODIndex, Second.LODIndex);
		});

	// 정렬 결과 반투명은 뒤쪽에 모이므로 첫 반투명 위치에서 두 패스를 나눈다.
	FirstTranslucentIndex = 0;
	while (FirstTranslucentIndex < RenderPackets.size()
		&& (RenderPackets[FirstTranslucentIndex].Material == nullptr
			|| RenderPackets[FirstTranslucentIndex].Material->BlendState == EBlendState::Opaque))
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
		if (RenderPacket.Mesh == nullptr || RenderPacket.Material == nullptr) continue;
		if (RenderPacket.Mesh != LastMesh || RenderPacket.LODIndex != LastLODIndex) {
			RenderCommand::BindMesh(RenderPacket.Mesh,RenderPacket.LODIndex);
		}
		if (RenderPacket.Material != LastMaterial) {
			BindMaterial(RenderPacket.Material);
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
			RenderPacket.IndexCount ? RenderPacket.IndexCount : RenderPacket.Mesh->GetIndexBuffer(RenderPacket.LODIndex)->GetIndexCount(),
			RenderPacket.StartIndex
		);
		LastMaterial = RenderPacket.Material;
		LastMesh = RenderPacket.Mesh;
		LastLODIndex = RenderPacket.LODIndex;
	}
}

FOcclusionMeasureResult FRenderer::MeasureOpaqueOcclusion(const FMatrix& ViewProjection)
{
	FOcclusionMeasureResult Result;
	const uint32 Count = FirstTranslucentIndex;
	if (Count == 0)
		return Result;

	const auto StartTime = std::chrono::high_resolution_clock::now();

	ID3D11Device* Device = RenderCommand::GetDevice();
	ID3D11DeviceContext* Context = RenderCommand::GetContext();

	// 같은 셰이더·같은 행렬로 다시 그리면 깊이가 비트 단위로 같으므로,
	// LESS_EQUAL이면 최종 깊이 버퍼에서 이 물체가 이긴 픽셀만 통과한다.
	if (!DepthLessEqualReadOnly)
	{
		D3D11_DEPTH_STENCIL_DESC Desc{};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (FAILED(Device->CreateDepthStencilState(&Desc, DepthLessEqualReadOnly.GetAddressOf())))
			return Result;
	}

	while (OcclusionQueries.Num() < Count)
	{
		const D3D11_QUERY_DESC QueryDesc{ D3D11_QUERY_OCCLUSION, 0 };
		ComPtr<ID3D11Query> Query;
		if (FAILED(Device->CreateQuery(&QueryDesc, Query.GetAddressOf())))
			return Result;
		OcclusionQueries.Add(std::move(Query));
	}

	RenderCommand::BindConstantBuffer(0, ViewCB.get(), EShaderBindFlagBits::Vertex);
	if (!bUsePerObjectSlots)
		RenderCommand::BindConstantBuffer(2, PerObjectCB.get(), EShaderBindFlagBits::Vertex);

	UStaticMesh* BoundMesh = nullptr;
	UMaterial* BoundMaterial = nullptr;
	uint8 BoundLOD = 0;

	// 1) 패킷마다 쿼리를 걸고 다시 그린다. DrawPackets와 같은 순서·같은 바인딩을 쓴다.
	// std::vector<bool>은 비트 압축이라 참조를 못 돌려주므로 uint8을 쓴다.
	TArray<uint8> bIssued;
	bIssued.Init(0, Count);
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const FRenderPacket& Packet = RenderPackets[Index];
		if (Packet.Mesh == nullptr || Packet.Material == nullptr) continue;

		if (Packet.Mesh != BoundMesh || Packet.LODIndex != BoundLOD)
			RenderCommand::BindMesh(Packet.Mesh, Packet.LODIndex);
		if (Packet.Material != BoundMaterial)
		{
			BindMaterial(Packet.Material);
			// BindMaterial이 바꾼 상태를 측정용으로 덮어쓴다.
			RenderCommand::SetBlendState(EBlendState::NoColorWrite);
			Context->OMSetDepthStencilState(DepthLessEqualReadOnly.Get(), 0);
		}

		if (bUsePerObjectSlots)
			RenderCommand::BindConstantBufferRange(2, PerObjectSlotCB.get(),
				Index * PerObjectSlotConstants, PerObjectSlotConstants, EShaderBindFlagBits::Vertex);
		else
			UpdatePerObjectConstants(Packet, ViewProjection);

		const uint32 IndexCount = Packet.IndexCount ? Packet.IndexCount : Packet.Mesh->GetIndexBuffer(Packet.LODIndex)->GetIndexCount();
		ID3D11Query* Query = OcclusionQueries[Index].Get();
		Context->Begin(Query);
		RenderCommand::DrawIndexed(IndexCount, Packet.StartIndex);
		Context->End(Query);
		bIssued[Index] = 1;

		BoundMesh = Packet.Mesh;
		BoundMaterial = Packet.Material;
		BoundLOD = Packet.LODIndex;
	}

	// 2) 결과를 기다려 모은다. 한 물체가 Section 여러 개로 나뉘면 하나라도 보이면 보이는 것으로 친다.
	std::unordered_map<const void*, bool> ObjectVisible;
	ObjectVisible.reserve(Count);
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		if (!bIssued[Index]) continue;
		const FRenderPacket& Packet = RenderPackets[Index];

		UINT64 Samples = 0;
		while (Context->GetData(OcclusionQueries[Index].Get(), &Samples, sizeof(Samples), 0) == S_FALSE) {}

		const uint32 IndexCount = Packet.IndexCount ? Packet.IndexCount : Packet.Mesh->GetIndexBuffer(Packet.LODIndex)->GetIndexCount();
		const uint64 Triangles = IndexCount / 3;
		const bool bVisible = Samples > 0;

		++Result.TotalDraws;
		Result.TotalTriangles += Triangles;
		if (bVisible)
		{
			++Result.VisibleDraws;
			Result.VisibleTriangles += Triangles;
		}

		// 프록시가 없는 패킷(빌보드 등)은 패킷 자체를 한 물체로 센다.
		const void* Key = Packet.Proxy ? static_cast<const void*>(Packet.Proxy) : static_cast<const void*>(&Packet);
		bool& bObjectVisible = ObjectVisible[Key];
		bObjectVisible = bObjectVisible || bVisible;
	}

	Result.TotalObjects = static_cast<uint32>(ObjectVisible.size());
	for (const auto& [Key, bVisible] : ObjectVisible)
		Result.VisibleObjects += bVisible ? 1 : 0;

	// 뒤따르는 Grid·반투명 패스를 위해 상태를 되돌린다.
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);

	Result.ElapsedMs = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - StartTime).count();
	Result.bValid = true;
	return Result;
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
	switch (RenderPacket.Material->ParamLayout)
	{
		case EMaterialParamLayout::StaticMesh:
		{
			break; // 라이팅 적용 시 제거

			const float TotalTime = EngineTimer::GetTotalTime();

			FStaticMeshMaterialParams Params{};
			Params.BaseColor = RenderPacket.Material->BaseColor;
			Params.UVOffset = RenderPacket.Material->UVScrollSpeed * TotalTime;
			Params.bOpaque = RenderPacket.Material->BlendState == EBlendState::Opaque ? 1.0f : 0.0f;

			RenderCommand::UpdateBufferData(RenderPacket.Material->ParamBuffer.get(), &Params, sizeof(FStaticMeshMaterialParams));
			RenderCommand::BindConstantBuffer(1, RenderPacket.Material->ParamBuffer.get(), EShaderBindFlagBits::Pixel);
			break;
		}
		case EMaterialParamLayout::ParticleSubUV:
		{
			if (RenderPacket.Material->ParamBuffer && RenderPacket.MaterialParamData != nullptr)
			{
				RenderCommand::UpdateBufferData(RenderPacket.Material->ParamBuffer.get(), RenderPacket.MaterialParamData, RenderPacket.MaterialParamDataSize);
				RenderCommand::BindConstantBuffer(1, RenderPacket.Material->ParamBuffer.get(), EShaderBindFlagBits::Pixel);
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
