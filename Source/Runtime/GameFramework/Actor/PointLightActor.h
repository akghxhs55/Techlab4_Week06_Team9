#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/PointLightComponent.h"

class APointLightActor : public AActor
{
	DECLARE_CLASS(APointLightActor, AActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	APointLightActor();
	virtual ~APointLightActor() override = default;

	virtual void DuplicateSubObjects() override;

	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }

private:
	UBillboardComponent* BillboardComponent = nullptr;
	UPointLightComponent* PointLightComponent = nullptr;
};
