#pragma once 
#include "PostProcessPass.h"
#include "PostProcessContext.h"
#include "Shader.h"
#include "PipelineState.h"

struct FogCB {
	FMatrix InverseViewProj;

	FVector CameraLocation;
	float FogDensity;

	FVector FogInscatteringColor;
	float FogHeight;

	float FogHeightFalloff;
	float FogStartDistance;
	float FogMaxOpacity;
	float FogMaxDistance; 

	float FogCutOffDistance;
	FVector _Padding0;
};

class FFogRenderPass : public IPostProcessPass {
public:
	FFogRenderPass() = default;
	virtual ~FFogRenderPass() override = default;

	FFogRenderPass(const FFogRenderPass&) = delete;
	FFogRenderPass& operator=(const FFogRenderPass&) = delete;

	FFogRenderPass(FFogRenderPass&&) = default;
	FFogRenderPass& operator=(FFogRenderPass&&) = default;

public:
	virtual void Init() override;
	// CBuffer 에 필요한 데이터 복사하기. 
	// 필요한 버퍼 SRV Set 하기
	// 필요한 PipelineState Bind 하기
	virtual void Set(PostProcessContext& Context) override;

private:
	FPipelineState FogPS;
	FogCB FogCBData;
	TUniquePtr<FConstantBuffer> DepthCB;
};