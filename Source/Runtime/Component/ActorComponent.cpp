#include "EnginePCH.h"

#include "GameFramework/Actor.h"

UActorComponent::~UActorComponent()
{
	if (Owner)
	{
		Owner->RemoveOwnedComponent(this);
	}
}

UActorComponent::UActorComponent(const UActorComponent& Other)
	: UObject(Other)
	, PrimaryComponentTick(Other.PrimaryComponentTick)
{
}

void UActorComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	PrimaryComponentTick.Target = this;
}