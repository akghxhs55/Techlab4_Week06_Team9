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

void AFogActor::DuplicateSubObjects()
{
	int32 FogIndex = -1;
	int32 BillboardIndex = -1;
	for (int32 i = 0; i < Components.Num(); ++i)
	{
		if (Components[i] == ExpHeightFogComponent)
		{
			FogIndex = i;
		}
		else if (Components[i] == BillboardComponent)
		{
			BillboardIndex = i;
		}
	}

	Super::DuplicateSubObjects();

	ExpHeightFogComponent = FogIndex != -1 ? Cast<UExpHeightFogComponent>(Components[FogIndex]) : nullptr;
	BillboardComponent = BillboardIndex != -1 ? Cast<UBillboardComponent>(Components[BillboardIndex]) : nullptr;
}
