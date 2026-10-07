#include "EnginePCH.h"
#include "FXAAPass.h"
#include "RenderResourceManager.h"
#include "RenderCommand.h"
#include "./../../Editor/LevelEditor/MultipleViewports/Adapter/MultipleViewportsAdapter.h"

void FFXAAPass::Init() {
	FXAAPipelineState.Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/FXAA.hlsl");
	FXAAPipelineState.BlendState = EBlendState::Opaque;
	FXAAPipelineState.DepthStencilState = EDepthStencilState::Default;
	FXAAPipelineState.RasterizerState = ERasterizerState::SolidBack;
	FXAAPipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

	D3D11_BUFFER_DESC Desc{};
	Desc.ByteWidth = sizeof(FXAACB);
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	FXAACBuffer = MakeUnique<FConstantBuffer>(RenderCommand::GetDevice(), Desc);
}

void FFXAAPass::Set(PostProcessContext& Context) {
	RenderCommand::BindPipelineState(FXAAPipelineState);

	auto  view = Context.ViewportAdapter->GetRenderView(Context.ViewIndex);

	FXAACB data{
		.InverseScreenSize = {1.f / Context.ColorBuffer->GetWidth(), 1.f / Context.ColorBuffer->GetHeight()},
		.FXAAThreshold = 0.125f,
		.FXAAThresholdMin = 0.0625f,
	};


	std::memcpy(RenderCommand::MapWriteDiscard(FXAACBuffer.get()), &data, sizeof(FXAACB));
	RenderCommand::BindConstantBuffer(0, FXAACBuffer.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	RenderCommand::Unmap(FXAACBuffer.get());

	RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);

	RenderCommand::BindShaderResource(0, Context.ColorBuffer, EShaderBindFlagBits::Pixel);

}
