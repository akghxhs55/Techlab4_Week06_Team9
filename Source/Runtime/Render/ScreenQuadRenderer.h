#pragma once 
#include "Texture2D.h"
#include "RenderingInfo.h"
#include "Shader.h"
#include "PipelineState.h"

#include "RenderResourceManager.h"

struct FScreenQuadCB
{
	FMatrix InverseProj; 
};

class FScreenQuadRenderer {
public:
	FScreenQuadRenderer() = default;
	~FScreenQuadRenderer() = default;

	FScreenQuadRenderer(const FScreenQuadRenderer&) = delete;
	FScreenQuadRenderer& operator=(const FScreenQuadRenderer&) = delete;

	FScreenQuadRenderer(FScreenQuadRenderer&&) = default;
	FScreenQuadRenderer& operator=(FScreenQuadRenderer&&) = default;

public:
	void Init();
	void Render(FTexture2D* TargetTexture, const FRenderingInfo& Sources, const FMatrix& proj);

private:
	FPipelineState Colorps;
	FPipelineState Depthps;

	TUniquePtr<FConstantBuffer> ScreenQuadCB;
};