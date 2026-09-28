#pragma once

#include "Engine/GameEngine.h"
#include "Engine/World.h"

class FImGuiRenderer;

class UBenchmarkEngine : public UGameEngine
{
	DECLARE_CLASS(UBenchmarkEngine, UGameEngine)

public:
	FEngineConfig GetConfig() const override;

	bool Init() override;
	void Tick(float DeltaTime) override;

private:
	// ImGui로 프로파일링 오버레이를 그린다.
	void DrawProfileOverlay();

	TUniquePtr<FImGuiRenderer> ImGuiRenderer;
};
