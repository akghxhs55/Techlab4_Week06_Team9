#include "EnginePCH.h"
#include "ScreenQuadRenderer.h"
#include "RenderCommand.h"

void FScreenQuadRenderer::Init() {
	ps.Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ScreenQuad.hlsl");
	ps.BlendState = EBlendState::Opaque;
	ps.DepthStencilState = EDepthStencilState::Default;
	ps.RasterizerState = ERasterizerState::SolidBack;
	ps.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
}

void FScreenQuadRenderer::Render(FTexture2D* TargetTexture, const FRenderingInfo& Sources) {
	RenderCommand::BindPipelineState(ps);
	RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);

	ID3D11RenderTargetView* targets[] = { TargetTexture->GetRTV() };

	// Quad Rendering 에 사용할 Texutre... 
	RenderCommand::GetContext()->OMSetRenderTargets(1, targets, nullptr);
	RenderCommand::BindShaderResource(0, Sources.ColorRenderTargets[0].Texture, EShaderBindFlagBits::Pixel);

	RenderCommand::Draw(3, 0);
	
	ID3D11ShaderResourceView* NullSRV{ nullptr };
	RenderCommand::GetContext()->PSSetShaderResources(0, 1, &NullSRV);
}
