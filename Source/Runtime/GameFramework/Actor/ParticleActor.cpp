#include "EnginePCH.h"
#include "ParticleActor.h"

// Todo: subuv
AParticleActor::AParticleActor()
{
	ParticleComponent = CreateDefaultSubobject<UParticleSubUVComponent>("UParticleSubUVComponent");
	SetRootComponent(ParticleComponent);

	ParticleComponent->SetSubUVSize(8, 8);
	ParticleComponent->SetFrameRate(12.0f);
}

AParticleActor::AParticleActor(const AParticleActor& Other)
	: AActor(Other)
	, ParticleComponent(Other.ParticleComponent)
{
}

void AParticleActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	for (UActorComponent* Component : GetComponents())
	{
		if (Component == ParticleComponent)
		{
			ParticleComponent = static_cast<UParticleSubUVComponent*>(Component);
			break;
		}
	}
}

UParticleSubUVComponent* AParticleActor::GetParticleComponent() const
{
	return static_cast<UParticleSubUVComponent*>(RootComponent);
}


