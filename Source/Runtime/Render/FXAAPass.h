#pragma once 
#include "PostProcessPass.h"
#include "PostProcessContext.h"
#include "Shader.h"
#include "PipelineState.h"

struct FXAACB {
	FVector2 InverseScreenSize; // { 1.0f / ScreenSize.x, 1.0f / ScreenSize.y }
	float FXAAThreshold; // 0.125f
	float FXAAThresholdMin; // 0.0625f

	FMatrix _Padding0;
};

class FFXAAPass : public IPostProcessPass {
public:
	FFXAAPass() = default;
	virtual ~FFXAAPass() override = default;

	FFXAAPass(const FFXAAPass&) = delete;
	FFXAAPass& operator=(const FFXAAPass&) = delete;

	FFXAAPass(FFXAAPass&&) = default;
	FFXAAPass& operator=(FFXAAPass&&) = default;

public:
	virtual void Init() override;
	// CBuffer 에 필요한 데이터 복사하기. 
	// 필요한 버퍼 SRV Set 하기
	// 필요한 PipelineState Bind 하기
	virtual void Set(PostProcessContext& Context) override;

private:
	FPipelineState FXAAPipelineState;

	TUniquePtr<FConstantBuffer> FXAACBuffer;
};