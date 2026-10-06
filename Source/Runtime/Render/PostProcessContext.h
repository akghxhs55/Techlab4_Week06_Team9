#pragma once 
#include "Texture2D.h"
#include "../Engine/World.h"
class FMultipleViewportsAdapter;

struct PostProcessContext {
	FTexture2D* ColorBuffer = nullptr;
	FTexture2D* DepthBuffer = nullptr;

	int32 ViewIndex = 0;
	
	FMultipleViewportsAdapter* ViewportAdapter = nullptr;
	FWorldContext* WorldContext = nullptr;
};