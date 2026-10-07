#include "EnginePCH.h"
#include "AkaActor.h"

AAkaActor::AAkaActor()
{
	// 1. SphereGlowComponent: Intensity 10.4, Radius 1.0, RadiusFallOff 1.2, Color Red (R 255 G 0 B 0)
	if (SphereGlowComponent)
	{
		SphereGlowComponent->SetColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
		SphereGlowComponent->SetIntensity(10.4f);
		SphereGlowComponent->SetRadius(1.0f);
		SphereGlowComponent->SetRadiusFallOff(1.2f);
	}

	// 2. PointLightComponent: 크고 밝은 레드 포인트 라이트
	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("UPointLightComponent");
	if (SphereGlowComponent)
	{
		PointLightComponent->SetupAttachment(SphereGlowComponent);
	}
	else
	{
		SetRootComponent(PointLightComponent);
	}
	PointLightComponent->SetLightColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	PointLightComponent->SetIntensity(85.0f);
	PointLightComponent->SetAttenuationRadius(30.0f);

	// 3. RotatingMovementComponent: PivotTranslation X = -10, Play 시에만 동작하도록 설정
	RotatingMovementComponent = CreateDefaultSubobject<URotatingMovementComponent>("URotatingMovementComponent");
	RotatingMovementComponent->SetPivotTranslation(FVector(-10.0f, 0.0f, 0.0f));
	RotatingMovementComponent->SetRotationRate(FRotator(0.0f, 180.0f, 0.0f));
	RotatingMovementComponent->PrimaryComponentTick.bTickInEditor = false;
}

void AAkaActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	RotatingMovementComponent = nullptr;
	PointLightComponent = nullptr;
	for (UActorComponent* Component : GetComponents())
	{
		if (URotatingMovementComponent* RMC = Cast<URotatingMovementComponent>(Component))
		{
			RotatingMovementComponent = RMC;
		}
		else if (UPointLightComponent* PLC = Cast<UPointLightComponent>(Component))
		{
			PointLightComponent = PLC;
		}
	}
}
