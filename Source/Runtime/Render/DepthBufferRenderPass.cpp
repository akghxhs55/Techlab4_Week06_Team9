#include "EnginePCH.h"
#include "DepthBufferRenderPass.h"
#include "RenderResourceManager.h"
#include "RenderCommand.h"
#include "./../../Editor/LevelEditor/MultipleViewports/Adapter/MultipleViewportsAdapter.h"

void FDepthBufferRenderPass::Init() {

	DepthPS.Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/DepthPass.hlsl");
	DepthPS.BlendState = EBlendState::Opaque;
	DepthPS.DepthStencilState = EDepthStencilState::Default;
	DepthPS.RasterizerState = ERasterizerState::SolidBack;
	DepthPS.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

	D3D11_BUFFER_DESC Desc{};
	Desc.ByteWidth = sizeof(DepthBufferCB);
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	DepthCB = MakeUnique<FConstantBuffer>(RenderCommand::GetDevice(), Desc);
}

void FDepthBufferRenderPass::Set(PostProcessContext& Context) {
	RenderCommand::BindPipelineState(DepthPS);

	auto view = Context.ViewportAdapter->GetRenderView(Context.ViewIndex);
	DepthBufferData.InverseProj = view.Projection.Inverse();

	std::memcpy(RenderCommand::MapWriteDiscard(DepthCB.get()), &DepthBufferData, sizeof(DepthBufferCB));
	RenderCommand::BindConstantBuffer(0, DepthCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	RenderCommand::Unmap(DepthCB.get());

	RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);

	RenderCommand::BindShaderResource(0, Context.DepthBuffer, EShaderBindFlagBits::Pixel);
}
