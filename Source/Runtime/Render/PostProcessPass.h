#pragma once 
#include  "PostProcessContext.h"

class IPostProcessPass {
public:
	virtual ~IPostProcessPass() {};
	

	virtual void Init() = 0; 
	// CBuffer 에 필요한 데이터 복사하기. 
	// 필요한 버퍼 SRV Set 하기
	// 필요한 PipelineState Bind 하기
	virtual void Set(PostProcessContext& Context) = 0;
};