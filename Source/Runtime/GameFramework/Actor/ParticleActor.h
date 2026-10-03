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
	virtual void DuplicateSubObjects() override;
	UParticleSubUVComponent* GetParticleComponent() const;

private:
	UParticleSubUVComponent* ParticleComponent;
};
