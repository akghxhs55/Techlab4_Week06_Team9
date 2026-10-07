#pragma once
#include "GameFramework/Actor.h"
#include "Component/SphereGlowComponent.h"

class ASphereGlowActor : public AActor
{
	DECLARE_CLASS(ASphereGlowActor, AActor)

public:
    ASphereGlowActor();
    USphereGlowComponent* GetSphereGlowComponent() const;

    virtual void DuplicateSubObjects() override;

protected:
    USphereGlowComponent* SphereGlowComponent = nullptr;
};

