#pragma once

#include <format>
#include "Editor/EditorUI/EditorPanel.h"

#include "Engine/World.h"

struct FTransform;

class FDetailsPanel : public IEditorPanel
{
public:
	FDetailsPanel() = default;
	~FDetailsPanel();

	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Details"; }

	void SetTarget(USceneComponent* InTargetOrNull) { Target = InTargetOrNull; }

	void SetWorld(UWorld* InWorld) { World = InWorld; }

	ImFont* GetCustomFont() { return CustomFont; }

private:
	void DrawComponentSection(AActor* Actor);
	void DrawSceneComponentNode(USceneComponent* Component);
	void DrawActorComponent(UActorComponent* Component);

	UWorld* World = nullptr;
	USceneComponent* Target = nullptr;
	ImFont* CustomFont = nullptr;
};

