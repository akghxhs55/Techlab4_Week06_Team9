#include "EnginePCH.h"
#include "ColorPostProcessPass.h"
#include "RenderResourceManager.h"
#include "RenderCommand.h"

void FColorProcessPass::Init() {
	ColorPS.Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ColorPass.hlsl");
	ColorPS.BlendState = EBlendState::Opaque;
	ColorPS.DepthStencilState = EDepthStencilState::Default;
	ColorPS.RasterizerState = ERasterizerState::SolidBack;
	ColorPS.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
}

void FColorProcessPass::Set(PostProcessContext& Context) {
	RenderCommand::BindPipelineState(ColorPS);

	RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);

	RenderCommand::BindShaderResource(0, Context.ColorBuffer, EShaderBindFlagBits::Pixel);
}
