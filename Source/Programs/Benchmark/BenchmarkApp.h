#pragma once

#include "Engine/GameEngine.h"
#include "Engine/World.h"

class UBenchmarkEngine : public UGameEngine
{
	DECLARE_CLASS(UBenchmarkEngine, UGameEngine)

public:
	FEngineConfig GetConfig() const override;

	bool Init() override;
	void Tick(float DeltaTime) override;

private:
};
