#pragma once

#include "GameFramework/Actor/SphereGlowActor.h"
#include "Component/RotatingMovementComponent.h"
#include "Component/PointLightComponent.h"

class AAoActor : public ASphereGlowActor
{
	DECLARE_CLASS(AAoActor, ASphereGlowActor)

	REFLECT_START(AAoActor)
	REFLECT_END()

public:
	AAoActor();
	virtual ~AAoActor() override = default;

	virtual void DuplicateSubObjects() override;

	URotatingMovementComponent* GetRotatingMovementComponent() const { return RotatingMovementComponent; }
	UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }

protected:
	URotatingMovementComponent* RotatingMovementComponent = nullptr;
	UPointLightComponent* PointLightComponent = nullptr;
};
