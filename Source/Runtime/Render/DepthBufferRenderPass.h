#pragma once 
#include "PostProcessPass.h"
#include "PostProcessContext.h"
#include "Shader.h"
#include "PipelineState.h"

struct DepthBufferCB {
	FMatrix InverseProj;
};

class FDepthBufferRenderPass : public IPostProcessPass {
public:
	FDepthBufferRenderPass() = default;
	virtual ~FDepthBufferRenderPass() override = default;

	FDepthBufferRenderPass(const FDepthBufferRenderPass&) = delete;
	FDepthBufferRenderPass& operator=(const FDepthBufferRenderPass&) = delete;
	
	FDepthBufferRenderPass(FDepthBufferRenderPass&&) = default;
	FDepthBufferRenderPass& operator=(FDepthBufferRenderPass&&) = default;

public:
	virtual void Init() override;
	// CBuffer 에 필요한 데이터 복사하기. 
	// 필요한 버퍼 SRV Set 하기
	// 필요한 PipelineState Bind 하기
	virtual void Set(PostProcessContext& Context) override;

private:
	FPipelineState DepthPS;
	DepthBufferCB DepthBufferData;
	TUniquePtr<FConstantBuffer> DepthCB;
};