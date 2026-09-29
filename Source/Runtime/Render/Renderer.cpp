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

bool FRenderer::Init()
{
	PerObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FPerObjectConstants));

	return true;
}

// 카메라의 ViewProjection을 공통 렌더 경로로 전달한다.
void FRenderer::RenderAll(TQueue<FRenderPacket>& InQueue, UCameraComponent* CameraComponent)
{
	RenderAll(InQueue, CameraComponent->GetViewProjectionMatrix());
}

// 불투명 우선·반투명 거리순으로 정렬해 View 행렬과 Section 범위로 그린다.
void FRenderer::RenderAll(TQueue<FRenderPacket>& InQueue, const FMatrix& ViewProjection)
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

void FRenderer::RenderQueueSorting(TQueue<FRenderPacket>& InQueue, const FMatrix& ViewProjection)
{
	SCOPE_CYCLE_COUNTER(STAT_RenderQueueSorting);

	RenderPackets.Reset();

	while (InQueue.IsEmpty() == false)
	{
		RenderPackets.Add(InQueue.Peek());
		InQueue.Dequeue();
	}

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
			return std::less<UStaticMesh*>{}( First.mesh, Second.mesh);
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

// 정렬된 패킷 중 [Begin, End) 범위를 View 행렬과 Section 범위로 그린다.
void FRenderer::DrawPackets(uint32 Begin, uint32 End, const FMatrix& ViewProjection)
{
	SCOPE_CYCLE_COUNTER(STAT_DrawRenderPackets);

	LastMesh = nullptr;
	LastMaterial = nullptr;
	uint8 LastLODIndex = 0;

	RenderCommand::BindConstantBuffer(0, PerObjectCB.get(), EShaderBindFlagBits::Vertex);

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
		UpdatePerObjectConstants(RenderPacket, ViewProjection);

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

	Constants.MVP = (RenderPacket.model * ViewProjection).GetTransposed();
	Constants.World = RenderPacket.model.GetTransposed();

	RenderCommand::UpdateBufferData(PerObjectCB.get(), &Constants);
}
