#pragma once

#include "GameFramework/Actor/SphereGlowActor.h"
#include "Component/RotatingMovementComponent.h"
#include "Component/PointLightComponent.h"

class AAkaActor : public ASphereGlowActor
{
	DECLARE_CLASS(AAkaActor, ASphereGlowActor)

	REFLECT_START(AAkaActor)
	REFLECT_END()

public:
	AAkaActor();
	virtual ~AAkaActor() override = default;

	virtual void DuplicateSubObjects() override;

	URotatingMovementComponent* GetRotatingMovementComponent() const { return RotatingMovementComponent; }
	UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }

protected:
	URotatingMovementComponent* RotatingMovementComponent = nullptr;
	UPointLightComponent* PointLightComponent = nullptr;
};
