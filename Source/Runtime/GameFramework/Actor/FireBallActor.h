#pragma once

#include "GameFramework/Actor.h"
#include "Component/SphereGlowComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/ProjectileMovementComponent.h"

class AFireBallActor : public AActor
{
	DECLARE_CLASS(AFireBallActor, AActor)

	REFLECT_START(AFireBallActor)
	REFLECT_END()

public:
	AFireBallActor();
	virtual ~AFireBallActor() override = default;

	virtual void DuplicateSubObjects() override;

	USphereGlowComponent* GetSphereGlowComponent() const { return SphereGlowComponent; }
	UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }
	UProjectileMovementComponent* GetProjectileMovementComponent() const { return ProjectileMovementComponent; }

private:
	USphereGlowComponent* SphereGlowComponent = nullptr;
	UPointLightComponent* PointLightComponent = nullptr;
	UProjectileMovementComponent* ProjectileMovementComponent = nullptr;
};
