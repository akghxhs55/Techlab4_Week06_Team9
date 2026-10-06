#include "EnginePCH.h"
#include "ParticleActor.h"

#include "Asset/AssetManager.h"

AParticleActor::AParticleActor()
{
	ParticleComponent = CreateDefaultSubobject<UParticleSubUVComponent>("UParticleSubUVComponent");
	SetRootComponent(ParticleComponent);

	ParticleComponent->SetSubUVSize(8, 8);
	ParticleComponent->SetFrameRate(12.0f);

	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	BillboardComponent->SetupAttachment(ParticleComponent);
	BillboardComponent->SetMaterial(0, UAssetManager::GetAssetByPath<UMaterial>("ParticleIcon"));
	BillboardComponent->SetEditorOnly(true);
	BillboardComponent->SetHideInDetails(true);
}

AParticleActor::AParticleActor(const AParticleActor& Other)
	: AActor(Other)
	, ParticleComponent(Other.ParticleComponent)
	, BillboardComponent(Other.BillboardComponent)
{
}

void AParticleActor::DuplicateSubObjects()
{
	int32 ParticleIndex = -1;
	int32 BillboardIndex = -1;
	for (int32 i = 0; i < GetComponents().Num(); ++i)
	{
		if (GetComponents()[i] == ParticleComponent)
		{
			ParticleIndex = i;
		}
		else if (GetComponents()[i] == BillboardComponent)
		{
			BillboardIndex = i;
		}
	}

	Super::DuplicateSubObjects();

	ParticleComponent = ParticleIndex != -1 ? Cast<UParticleSubUVComponent>(GetComponents()[ParticleIndex]) : nullptr;
	BillboardComponent = BillboardIndex != -1 ? Cast<UBillboardComponent>(GetComponents()[BillboardIndex]) : nullptr;
}
