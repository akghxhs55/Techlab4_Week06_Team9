#pragma once

struct FViewCamera;
class UWorld;

class FJsonArchive
{
public:
	static bool SaveWorld(UWorld* World, const FViewCamera* Camera, const FString& Path);
	static bool LoadWorld(UWorld* World, FViewCamera* OutCamera, const FString& Path);
};