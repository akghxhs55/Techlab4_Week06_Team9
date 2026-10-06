#pragma once

#include "GameFramework/Actor.h"
#include "Component/ParticleSubUVComponent.h"

// Todo: subuv
class AParticleActor : public AActor
{
	DECLARE_CLASS(AParticleActor, AActor)

	REFLECT_START(ClassName)
		REFLECT_END()

public:
	AParticleActor();
	AParticleActor(const AParticleActor& Other);

	UParticleSubUVComponent* GetParticleComponent() const { return ParticleComponent; }
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }

	virtual void DuplicateSubObjects() override;

private:
	UParticleSubUVComponent* ParticleComponent;
	UBillboardComponent* BillboardComponent;
};
