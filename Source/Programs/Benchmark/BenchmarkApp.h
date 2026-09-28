#pragma once

#include "Engine/Engine.h"
#include "Engine/World.h"

class UBenchmarkEngine : public UEngine
{
	DECLARE_CLASS(UBenchmarkEngine, UEngine)

public:
	FEngineConfig GetConfig() const override;

	bool Init() override;
	void Tick(float DeltaTime) override;

private:
	UWorld* World = nullptr;
};
