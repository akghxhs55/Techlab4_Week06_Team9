#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Property.h"
#include "Render/Renderer.h"
#include "Asset/LOD/StaticMeshLODGenerator.h"

#include <filesystem>

enum class EAssetType
{

};

class UStaticMesh;
class FShader;

namespace fs = std::filesystem;

class UAssetManager : public UObject
{
	DECLARE_CLASS(UAssetManager, UObject)
private:
	UAssetManager() = default;

	UAssetManager(const UAssetManager& src) =  delete;
	UAssetManager& operator= (const UAssetManager& src) = delete;

public:
	static UAssetManager& Get();
	static void Init();
	static void Shutdown();

	void ScanAssets(const fs::path& AssetRoot);
	void LoadAsset(const FString& Key, const FString& Path);

	void CreateDefaultTextures();
	void CreateDefaultMeshes();
	void CreateDefaultMaterial();
	void CreateParticleMaterial();

	template <typename T>
	static T* GetAssetByPath(const FString& Path)
	{
		URenderAsset** Found = Get().AssetMap.FindOrNull(Path);

		if (Found == nullptr || *Found == nullptr || !(*Found)->IsA<T>())
		{
			return nullptr;
		}
		return Cast<T>(*Found);
	}

	void RegisterAsset(const FString& Key, URenderAsset* Asset);

	//UStaticMesh* GetMesh(FString InName);
	//UStaticMesh* GetMesh(EPrimitiveType Type); // 오버로드(수정 중)

	UTexture2D* LoadTexture(const FString& InPath);
	UFont* LoadFontAtlas(const FString& JsonPath, const FString& AtlasTexturePath);
	static UStaticMesh* LoadObjStaticMesh(const FString& Path);

	FLODGenerateResult GenerateStaticMeshLODs(UStaticMesh& Mesh,const FLODGenerateRequest& Request);

private:
	TMap<FString, FString> AssetPathMap;
	TMap<FString, URenderAsset*> AssetMap;

	//TMap<FString, UStaticMesh*> MeshMap;	// KEY: FILE NAME OR 쉐입 첫글자 대문자
	TMap<FString, TUniquePtr<FShader>> ShaderMap;	// KEY: FILE NAME

	//TMap<FString, UTexture2D*> TextureMap;
};