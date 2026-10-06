#include "EnginePCH.h"
#include "PointLightActor.h"
#include "Asset/AssetManager.h"

APointLightActor::APointLightActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	SetRootComponent(BillboardComponent);
	if (UMaterial* IconMat = UAssetManager::GetAssetByPath<UMaterial>("PointlightIcon"))
	{
		BillboardComponent->SetMaterial(0, IconMat);
	}

	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("UPointLightComponent");
	PointLightComponent->SetupAttachment(BillboardComponent);
}

void APointLightActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	BillboardComponent = Cast<UBillboardComponent>(RootComponent);
	for (UActorComponent* Component : GetComponents())
	{
		if (UPointLightComponent* PLC = Cast<UPointLightComponent>(Component))
		{
			PointLightComponent = PLC;
			break;
		}
	}
}

