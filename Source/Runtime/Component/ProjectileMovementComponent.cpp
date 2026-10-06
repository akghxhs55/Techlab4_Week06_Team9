#include "EnginePCH.h"
#include "ProjectileMovementComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/Actor.h"

UProjectileMovementComponent::UProjectileMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.Target = this;
}

void UProjectileMovementComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	PrimaryComponentTick.Target = this;
	UpdatedComponent = nullptr;
}

void UProjectileMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!UpdatedComponent && GetOwner())
	{
		UpdatedComponent = GetOwner()->GetRootComponent();
	}

	if (UpdatedComponent)
	{
		// 액터의 로컬 X축 (전방 Forward 벡터)
		const FVector ForwardVector = UpdatedComponent->GetTransform().GetForward();
		Velocity = ForwardVector * InitialSpeed;
	}
}

void UProjectileMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (!UpdatedComponent)
	{
		if (GetOwner())
		{
			UpdatedComponent = GetOwner()->GetRootComponent();
		}
		if (!UpdatedComponent)
		{
			return;
		}
	}
	
	if (ProjectileGravityScale != 0.0f)
	{
		Velocity.Z -= StandardGravity * ProjectileGravityScale * DeltaTime;
	}

	if (MaxSpeed > 0.0f)
	{
		const float SpeedSq = Velocity.X * Velocity.X + Velocity.Y * Velocity.Y + Velocity.Z * Velocity.Z;
		if (SpeedSq > MaxSpeed * MaxSpeed)
		{
			const float CurrentSpeed = std::sqrt(SpeedSq);
			if (CurrentSpeed > 0.0001f)
			{
				Velocity = (Velocity / CurrentSpeed) * MaxSpeed;
			}
		}
	}

	const FVector DeltaLocation = Velocity * DeltaTime;
	const FVector CurrentLocation = UpdatedComponent->GetRelativeLocation();
	UpdatedComponent->SetRelativeLocation(CurrentLocation + DeltaLocation);
}
