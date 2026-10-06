#pragma once

#include "Engine/World.h"

struct FViewCamera;

class FEditorFileUtils
{
public:
	static bool NewScene(UWorld* World);
	static bool SaveScene(UWorld* World, const FViewCamera* Camera);
	static bool SaveSceneAs(UWorld* World, const FViewCamera* Camera);
	static bool LoadScene(UWorld* World, FViewCamera* OutCamera);

private:
	static FString OpenSaveSceneDialog();
	static FString OpenLoadSceneDialog();

	static FString CurrentScenePath;
};

