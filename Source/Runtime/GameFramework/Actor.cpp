#include "EnginePCH.h"
#include "Actor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "../Component/UExpHeightFogComponent.h"

AActor::AActor()
{
	PrimaryActorTick.Target = this;
}

AActor::~AActor()
{
	TArray<UActorComponent*> ToDelete = Components;
	Components.Reset();
	RootComponent = nullptr;

	for (UActorComponent* Component : ToDelete)
	{
		delete Component;
	}
}

AActor::AActor(const AActor& Other)
	: UObject(Other)
	, Components(Other.Components)
	, PrimaryActorTick(Other.PrimaryActorTick)
	, RootComponent(Other.RootComponent)
{
}

void AActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	TMap<USceneComponent*, USceneComponent*> OldToNewMap;
	// Duplicate all components
	for (UActorComponent*& Component : Components)
	{
		assert(Component); // Component should not be nullptr

		UActorComponent* OldComponent = Component;
		UActorComponent* NewComponent = Cast<UActorComponent>(OldComponent->Duplicate(OldComponent->GetClass()));

		NewComponent->SetOwner(this);

		if (OldComponent->IsA<USceneComponent>())
		{
			USceneComponent* OldSceneComponent = Cast<USceneComponent>(OldComponent);
			USceneComponent* NewSceneComponent = Cast<USceneComponent>(NewComponent);
			OldToNewMap.Add(OldSceneComponent, NewSceneComponent);
		}

		Component = NewComponent;
	}

	// Re-establish attachment relationships
	for (UActorComponent* Component : Components)
	{
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			if (USceneComponent* OldParent = SceneComponent->GetAttachParent())
			{
				if (USceneComponent** NewParentPtr = OldToNewMap.Find(OldParent))
				{
					SceneComponent->SetupAttachment(*NewParentPtr);
				}
			}
		}
	}

	// Reset the root component if it was duplicated
	if (USceneComponent** NewRootComponentPtr = OldToNewMap.Find(RootComponent))
	{
		RootComponent = *NewRootComponentPtr;
	}

	PrimaryActorTick.Target = this;

	// World and Level are set by the caller (UWorld::DuplicateSubObjects)

	RegisterAllActorTickFunctions(true);
}

void AActor::BeginPlay()
{
	//if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(RootComponent))
	//{
	//	World->AddPrimitive(Cast<UPrimitiveComponent>(RootComponent));
	//}

	for (UActorComponent* Component : Components)
	{
		Component->BeginPlay();
	}

	RegisterAllActorTickFunctions(true);
}

UActorComponent* AActor::AddComponentByClass(UClass* Class, bool bManualAttachment)
{
	UActorComponent* Component = CastChecked<UActorComponent>(FObjectFactory::ConstructObject(Class, this));
	Component->SetOwner(this);
	Components.Add(Component);

	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
	{
		if (World)
		{
			World->GetScene().AddPrimitive(Primitive);
		}
	}

	if (UExpHeightFogComponent* FogComponent = Cast<UExpHeightFogComponent>(Component))
	{
		if (World)
		{
			World->GetScene().AddFog(FogComponent);
		}
	}

	if (!bManualAttachment)
	{
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			if (!RootComponent)
			{
				RootComponent = SceneComponent;
			}
			else
			{
				SceneComponent->SetupAttachment(RootComponent);
			}
		}
	}

	return Component;
}

void AActor::RegisterAllActorTickFunctions(bool bRegister)
{
	if (bRegister && !World)
		return;

	// bCanEverTick이 꺼진 함수는 등록하지 않으므로 정적 메시 액터는 매 프레임 순회 대상에서 빠진다.
	auto Apply = [&](FTickFunction& Function)
		{
			if (bRegister)
				Function.RegisterTickFunction(World->GetTickTaskManager());
			else
				Function.UnRegisterTickFunction();
		};

	Apply(PrimaryActorTick);
	for (UActorComponent* Component : Components)
	{
		if (Component)
			Apply(Component->PrimaryComponentTick);
	}
}

void AActor::RemoveOwnedComponent(UActorComponent* Component)
{
	for (uint32 i = 0; i < Components.Num(); ++i)
	{
		if (Components[i] == Component)
		{
			Components.RemoveAt(i, 1);
			break;
		}
	}

	if (RootComponent == Component)
	{
		RootComponent = nullptr;
	}
}

FVector AActor::GetActorLocation() const
{
	if (RootComponent)
	{
		return RootComponent->GetWorldLocation();
	}
	return FVector::ZeroVector;
}

FRotator AActor::GetActorRotation() const
{
    if (RootComponent)
    {
        // USceneComponent의 GetWorldRotation() 호출
        return RootComponent->GetWorldRotation();
    }
    return FRotator(0, 0, 0);
}

FVector AActor::GetActorScale3D() const
{
	if (RootComponent)
	{
		return RootComponent->GetWorldScale3D();
	}
	return FVector::OneVector;
}

//FQuat AActor::GetActorQuat() const
//{
//    if (RootComponent)
//    {
//        return FQuat(RootComponent->GetWorldRotation());
//    }
//    return FQuat::Identity;
//}

FTransform AActor::GetActorTransform() const
{
	if (RootComponent)
	{
		return FTransform(
			RootComponent->GetWorldRotation(),
			RootComponent->GetWorldLocation(),
			RootComponent->GetWorldScale3D()
		);

		// return FTransform(RootComponent->GetWorldMatrix());
	}
	return FTransform::Identity;
}

bool AActor::Destroy()
{
	if (!World)
		return false;

	return World->DestroyActor(this);
}