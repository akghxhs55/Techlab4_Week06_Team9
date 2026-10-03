#include "EnginePCH.h"
#include "ScreenQuadRenderer.h"
#include "RenderCommand.h"
#include "Buffer.h"
#include <cstring>

void FScreenQuadRenderer::Init() {
	Colorps.Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ScreenQuad.hlsl");
	Colorps.BlendState = EBlendState::Opaque;
	Colorps.DepthStencilState = EDepthStencilState::Default;
	Colorps.RasterizerState = ERasterizerState::SolidBack;
	Colorps.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

	Depthps.Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ScreenQuad_Depth.hlsl");
	Depthps.BlendState = EBlendState::Opaque;
	Depthps.DepthStencilState = EDepthStencilState::Default;
	Depthps.RasterizerState = ERasterizerState::SolidBack;
	Depthps.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	

	D3D11_BUFFER_DESC Desc{};
	Desc.ByteWidth = sizeof(FScreenQuadCB);
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	ScreenQuadCB = MakeUnique<FConstantBuffer>(RenderCommand::GetDevice(), Desc);

}

void FScreenQuadRenderer::Render(FTexture2D* TargetTexture, const FRenderingInfo& Sources, const FMatrix& proj) {

	Sources.RenderBufferType == ERenderBuffer::Depth ? RenderCommand::BindPipelineState(Depthps) : RenderCommand::BindPipelineState(Colorps);

	static FScreenQuadCB cb;
	cb.InverseProj = proj.Inverse();


	if (Sources.RenderBufferType == ERenderBuffer::Depth) {
		std::memcpy(RenderCommand::MapWriteDiscard(ScreenQuadCB.get()), &cb, sizeof(FScreenQuadCB));
		RenderCommand::BindConstantBuffer(0, ScreenQuadCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
		RenderCommand::Unmap(ScreenQuadCB.get());
	}

	RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);	
	ID3D11RenderTargetView* targets[] = { TargetTexture->GetRTV() };

	// Quad Rendering 에 사용할 Texutre... 

	RenderCommand::GetContext()->OMSetRenderTargets(1, targets, nullptr);
	
	FTexture2D* RenderBuffer = Sources.RenderBufferType == ERenderBuffer::Depth ? Sources.DepthStencil.Texture : Sources.ColorRenderTargets[0].Texture;
	RenderCommand::BindShaderResource(0, RenderBuffer, EShaderBindFlagBits::Pixel);

	RenderCommand::Draw(3, 0);
	
	ID3D11ShaderResourceView* NullSRV{ nullptr };
	RenderCommand::GetContext()->PSSetShaderResources(0, 1, &NullSRV);
}
