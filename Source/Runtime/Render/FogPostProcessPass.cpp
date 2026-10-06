#include "EnginePCH.h"
#include "FogPostProcessPass.h"
#include "RenderResourceManager.h"
#include "RenderCommand.h"
#include "./../../Editor/LevelEditor/MultipleViewports/Adapter/MultipleViewportsAdapter.h"
#include "../Component/UExpHeightFogComponent.h"


void FFogRenderPass::Init() {
	FogPS.Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/FogPass.hlsl");
	FogPS.BlendState = EBlendState::Opaque;
	FogPS.DepthStencilState = EDepthStencilState::Default;
	FogPS.RasterizerState = ERasterizerState::SolidBack;
	FogPS.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

	D3D11_BUFFER_DESC Desc{};
	Desc.ByteWidth = sizeof(FogCB);
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	DepthCB = MakeUnique<FConstantBuffer>(RenderCommand::GetDevice(), Desc);
}

void FFogRenderPass::Set(PostProcessContext& Context) {
	RenderCommand::BindPipelineState(FogPS);

	FogData data = Context.WorldContext->World->GetScene().ExpHeightFogs[0]->GetFogData();

	//FogData data = {
	//	.FogDensity = 0.1f,
	//	.FogHeightFalloff = 0.2f,
	//	.FogHeightStart = 0.0f,
	//	.FogInscatteringLuminance = FVector(1.f, 0.0f, 0.0f),
	//	.FogMaxOpacity = 1.0f,
	//	.StartDistance = 0.0f,
	//	.EndDistance = 1000.0f,
	//	.FogCutOffDistance = 1000.0f
	//};
	auto view = Context.ViewportAdapter->GetRenderView(Context.ViewIndex);

	FogCBData.InverseViewProj = view.ViewProjection.Inverse();
	FogCBData.CameraLocation = view.CameraLocation;
	FogCBData.FogDensity = data.FogDensity;
	FogCBData.FogInscatteringColor = FVector{ data.FogInscatteringLuminance.X, data.FogInscatteringLuminance.Y, data.FogInscatteringLuminance.Z };
	FogCBData.FogHeight = data.FogHeightStart;
	FogCBData.FogHeightFalloff = data.FogHeightFalloff;
	FogCBData.FogStartDistance = data.StartDistance;
	FogCBData.FogMaxOpacity = data.FogMaxOpacity;
	FogCBData.FogMaxDistance = data.EndDistance;
	FogCBData.FogCutOffDistance = data.FogCutOffDistance;


	std::memcpy(RenderCommand::MapWriteDiscard(DepthCB.get()), &FogCBData, sizeof(FogCB));
	RenderCommand::BindConstantBuffer(0, DepthCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
	RenderCommand::Unmap(DepthCB.get());

	RenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);

	RenderCommand::BindShaderResource(0, Context.ColorBuffer, EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(1, Context.DepthBuffer, EShaderBindFlagBits::Pixel);

}
