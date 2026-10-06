#include "EnginePCH.h"
#include "FireBallActor.h"

AFireBallActor::AFireBallActor()
{
	// 1. Root: SphereGlowComponent
	SphereGlowComponent = CreateDefaultSubobject<USphereGlowComponent>("USphereGlowComponent");
	SetRootComponent(SphereGlowComponent);

	// 2. Child: PointLightComponent (Attached to Root)
	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("UPointLightComponent");
	PointLightComponent->SetupAttachment(SphereGlowComponent);
	PointLightComponent->SetLightColor(FVector4(1.0f, 0.4f, 0.1f, 1.0f));
	PointLightComponent->SetIntensity(2.0f);
	PointLightComponent->SetAttenuationRadius(15.0f);

	// 3. Logic: ProjectileMovementComponent
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>("UProjectileMovementComponent");
	ProjectileMovementComponent->SetInitialSpeed(20.0f);
	ProjectileMovementComponent->SetMaxSpeed(50.0f);
	ProjectileMovementComponent->SetGravityScale(1.0f);
}

void AFireBallActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	SphereGlowComponent = Cast<USphereGlowComponent>(RootComponent);
	for (UActorComponent* Component : GetComponents())
	{
		if (UPointLightComponent* PLC = Cast<UPointLightComponent>(Component))
		{
			PointLightComponent = PLC;
		}
		else if (UProjectileMovementComponent* PMC = Cast<UProjectileMovementComponent>(Component))
		{
			ProjectileMovementComponent = PMC;
		}
	}
}
