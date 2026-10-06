#include "EnginePCH.h"
#include "ScreenQuadRenderer.h"
#include "RenderCommand.h"
#include "../../Editor/Viewports/ViewportsPanel.h"
#include "../../Editor/LevelEditor/MultipleViewports/Adapter/MultipleViewportsAdapter.h"
#include "Buffer.h"
#include <cstring>

#include "ColorPostProcessPass.h"
#include "DepthBufferRenderPass.h"
#include "FogPostProcessPass.h"

void FPostProcessor::Init() {
	ColorPass = MakeUnique<FColorProcessPass>();
	ColorPass->Init();

	DepthPass = MakeUnique<FDepthBufferRenderPass>();
	DepthPass->Init();

	FogPass = MakeUnique<FFogRenderPass>();
	FogPass->Init();
}

void FPostProcessor::Render(int32 ViewIndex, FViewportsPanel* viewPorts, FMultipleViewportsAdapter* adapter, FWorldContext* WorldContext) {
	PostProcessContext Context;
	auto& info = viewPorts->GetRenderingInfo(ViewIndex);

	Context.ColorBuffer = info.ColorRenderTargets[0].Texture;
	Context.DepthBuffer = info.DepthStencil.Texture;
	Context.ViewIndex = ViewIndex;
	Context.ViewportAdapter = adapter;
	Context.WorldContext = WorldContext;



	ID3D11RenderTargetView* rts[] = { 
		viewPorts->GetViewRenderTarget1(ViewIndex)->GetRTV(), 
		viewPorts->GetViewRenderTarget2(ViewIndex)->GetRTV() , 
		viewPorts->GetFinalRenderTarget(ViewIndex)->GetRTV() 
	};


	if (info.RenderBufferType == ERenderBuffer::Color) {
		if (not Context.WorldContext->World->GetScene().ExpHeightFogs.empty()) {
			RenderCommand::GetContext()->OMSetRenderTargets(1, &rts[0], nullptr);
			ColorPass->Set(Context);

			RenderCommand::Draw(3, 0);
			
			Context.ColorBuffer = viewPorts->GetViewRenderTarget1(ViewIndex);
			RenderCommand::GetContext()->OMSetRenderTargets(1, &rts[2], nullptr);
			FogPass->Set(Context);

			RenderCommand::Draw(3, 0);
		}
		else {
			RenderCommand::GetContext()->OMSetRenderTargets(1, &rts[2], nullptr);
			ColorPass->Set(Context);

			RenderCommand::Draw(3, 0);

		}
		
	}
	else if (info.RenderBufferType == ERenderBuffer::Depth) {
		RenderCommand::GetContext()->OMSetRenderTargets(1, &rts[2], nullptr);
		DepthPass->Set(Context);

		RenderCommand::Draw(3, 0);
	}

	ID3D11ShaderResourceView* NullSRV{ nullptr };
	RenderCommand::GetContext()->PSSetShaderResources(0, 1, &NullSRV);
	
	RenderCommand::GetContext()->OMSetRenderTargets(1, &rts[2], info.DepthStencil.Texture->GetDSV());
}

//void FPostProcessor::Render(FTexture2D* TargetTexture, const FRenderingInfo& Sources, const FMatrix& proj) {
//
//
//
//
//	Sources.RenderBufferType == ERenderBuffer::Depth ? RenderCommand::BindPipelineState(Depthps) : RenderCommand::BindPipelineState(Colorps);
//
//	static FScreenQuadCB cb;
//	cb.InverseProj = proj.Inverse();
//
//	
//
//	if (Sources.RenderBufferType == ERenderBuffer::Depth) {
//		std::memcpy(RenderCommand::MapWriteDiscard(ScreenQuadCB.get()), &cb, sizeof(FScreenQuadCB));
//		RenderCommand::BindConstantBuffer(0, ScreenQuadCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
//		RenderCommand::Unmap(ScreenQuadCB.get());
//	}
//
//	RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);	
//	ID3D11RenderTargetView* targets[] = { TargetTexture->GetRTV() };
//
//	RenderCommand::GetContext()->OMSetRenderTargets(1, targets, nullptr);
//	
//	FTexture2D* RenderBuffer = Sources.RenderBufferType == ERenderBuffer::Depth ? Sources.DepthStencil.Texture : Sources.ColorRenderTargets[0].Texture;
//	RenderCommand::BindShaderResource(0, RenderBuffer, EShaderBindFlagBits::Pixel);
//
//	RenderCommand::Draw(3, 0);
//	
//	ID3D11ShaderResourceView* NullSRV{ nullptr };
//	RenderCommand::GetContext()->PSSetShaderResources(0, 1, &NullSRV);
//	
//
//	// 다음 Render Overlay 를 위해 다시 깊이 버퍼를 돌려준다. 
//	RenderCommand::GetContext()->OMSetRenderTargets(1, targets, Sources.DepthStencil.Texture->GetDSV()); 
//}
