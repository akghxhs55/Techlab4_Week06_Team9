#include "EnginePCH.h"
#include "FogActor.h"

#include "Asset/AssetManager.h"

AFogActor::AFogActor()
{
	ExpHeightFogComponent = CreateDefaultSubobject<UExpHeightFogComponent>("UExpHeightFogComponent");
	SetRootComponent(ExpHeightFogComponent);

	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	BillboardComponent->SetupAttachment(RootComponent);
	BillboardComponent->SetMaterial(0, UAssetManager::GetAssetByPath<UMaterial>("FogIcon"));
	BillboardComponent->SetEditorOnly(true);
	BillboardComponent->SetHideInDetails(true);
}