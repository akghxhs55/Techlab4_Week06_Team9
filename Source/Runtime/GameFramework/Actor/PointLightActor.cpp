#include "EnginePCH.h"
#include "PointLightActor.h"
#include "Asset/AssetManager.h"

APointLightActor::APointLightActor()
{
	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("UPointLightComponent");
	SetRootComponent(PointLightComponent);

	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	BillboardComponent->SetupAttachment(PointLightComponent);
	if (UMaterial* IconMat = UAssetManager::GetAssetByPath<UMaterial>("PointlightIcon"))
	{
		BillboardComponent->SetMaterial(0, IconMat);
	}
	BillboardComponent->SetEditorOnly(true);
	BillboardComponent->SetHideInDetails(true);
}

void APointLightActor::DuplicateSubObjects()
{
	int32 PointLightIndex = -1;
	int32 BillboardIndex = -1;
	for (int32 i = 0; i < GetComponents().Num(); ++i)
	{
		if (GetComponents()[i] == PointLightComponent)
		{
			PointLightIndex = i;
		}
		else if (GetComponents()[i] == BillboardComponent)
		{
			BillboardIndex = i;
		}
	}

	Super::DuplicateSubObjects();

	PointLightComponent = PointLightIndex != -1 ? Cast<UPointLightComponent>(GetComponents()[PointLightIndex]) : nullptr;
	BillboardComponent = BillboardIndex != -1 ? Cast<UBillboardComponent>(GetComponents()[BillboardIndex]) : nullptr;
}

