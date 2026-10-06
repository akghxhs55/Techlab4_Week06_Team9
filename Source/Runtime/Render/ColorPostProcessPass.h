#pragma once 
#include "PostProcessPass.h"
#include "PostProcessContext.h"
#include "Shader.h"
#include "PipelineState.h"


class FColorProcessPass : public IPostProcessPass {
public:
	FColorProcessPass() = default;
	virtual ~FColorProcessPass() override = default;

	FColorProcessPass(const FColorProcessPass&) = delete;
	FColorProcessPass& operator=(const FColorProcessPass&) = delete;

	FColorProcessPass(FColorProcessPass&&) = default;
	FColorProcessPass& operator=(FColorProcessPass&&) = default;

public:
	virtual void Init() override;
	// CBuffer 에 필요한 데이터 복사하기. 
	// 필요한 버퍼 SRV Set 하기
	// 필요한 PipelineState Bind 하기
	virtual void Set(PostProcessContext& Context) override;

private:
	FPipelineState ColorPS;
};