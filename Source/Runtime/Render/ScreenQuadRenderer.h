#pragma once 
#include "Texture2D.h"
#include "RenderingInfo.h"
#include "Shader.h"
#include "PipelineState.h"

#include "RenderResourceManager.h"
#include "../Component/UExpHeightFogComponent.h"

#include "PostProcessContext.h"
#include "PostProcessPass.h"
#include "FXAAPass.h"

// PostProcessor 에서 관리해 줄 것들
// 렌더 타겟 ping-pong, Screen Triangle Draw Call, 모든 Pass 끝나고 깊이 버퍼 다시 되돌려주기
// Pass 분기 
// 깊이 버퍼 렌더 모드 분기 는 EditorEngine 으로 빼자. 
// 어떤 Pass 를 할지 분기하는 것은 PostProcessor ( 기존 ScreenQuadRenderer ) 에서 하자.

// 나머지 부분은 IPostProcessPass 가 수행. 
// PostProcess Context 에 넣을 것들..
// Color Buffer(s), Depth Buffer, View Index, FMultipleViewportsAdapter, FWorldContext* 

// 기본 ColorPass, DepthBufferPass, FogPass, FXAAPass 

class FViewportsPanel;

class FPostProcessor {
public:
	FPostProcessor() = default;
	~FPostProcessor() = default;

	FPostProcessor(const FPostProcessor&) = delete;
	FPostProcessor& operator=(const FPostProcessor&) = delete;

	FPostProcessor(FPostProcessor&&) = default;
	FPostProcessor& operator=(FPostProcessor&&) = default;

public:
	void Init();
	// 현재 World 에 맞는 WorldContext 를 넣어 줄 것
	void Render(int32 ViewIndex, FViewportsPanel* viewPorts, FMultipleViewportsAdapter* adapter, FWorldContext* WorldContext);

	void SetAAEnabled(bool bEnabled) { AAEnabled = bEnabled; }
private:
	TUniquePtr<IPostProcessPass> ColorPass;
	TUniquePtr<IPostProcessPass> DepthPass;
	TUniquePtr<IPostProcessPass> FogPass;
	TUniquePtr<IPostProcessPass> FXAAPass;

	bool AAEnabled = true;
};