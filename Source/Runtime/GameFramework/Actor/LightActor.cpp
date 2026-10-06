#include "EnginePCH.h"
#include "LightActor.h"

#include "Asset/AssetManager.h"

ALightActor::ALightActor()
{
	SpotLightComponent = CreateDefaultSubobject<USpotLightComponent>("USpotLightComponent");
	SetRootComponent(SpotLightComponent);
	
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	BillboardComponent->SetupAttachment(SpotLightComponent);
	BillboardComponent->SetMaterial(0, UAssetManager::GetAssetByPath<UMaterial>("SpotlightIcon"));
	BillboardComponent->SetEditorOnly(true);
	BillboardComponent->SetHideInDetails(true);
}

void ALightActor::DuplicateSubObjects()
{
	int32 SpotLightIndex = -1;
	int32 BillboardIndex = -1;
	for (int32 i = 0; i < Components.Num(); ++i)
	{
		if (Components[i] == SpotLightComponent)
		{
			SpotLightIndex = i;
		}
		else if (Components[i] == BillboardComponent)
		{
			BillboardIndex = i;
		}
	}

	Super::DuplicateSubObjects();

	SpotLightComponent = SpotLightIndex != -1 ? Cast<USpotLightComponent>(Components[SpotLightIndex]) : nullptr;
	BillboardComponent = BillboardIndex != -1 ? Cast<UBillboardComponent>(Components[BillboardIndex]) : nullptr;
}
