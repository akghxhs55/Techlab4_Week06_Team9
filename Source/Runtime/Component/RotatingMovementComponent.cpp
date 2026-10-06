#include "EnginePCH.h"
#include "RotatingMovementComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/Actor.h"

URotatingMovementComponent::URotatingMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.Target = this;
}

void URotatingMovementComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	PrimaryComponentTick.Target = this;
	UpdatedComponent = nullptr;
}

void URotatingMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!UpdatedComponent && GetOwner())
	{
		UpdatedComponent = GetOwner()->GetRootComponent();
	}
}

void URotatingMovementComponent::TickComponent(float DeltaTime)
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

	if (RotationRate.Pitch == 0.0f && RotationRate.Yaw == 0.0f && RotationRate.Roll == 0.0f)
	{
		return;
	}

	const FRotator DeltaRotation = RotationRate * DeltaTime;
	const FQuat DeltaQuat = DeltaRotation.Quaternion();

	const bool bHasPivot = (PivotTranslation.X != 0.0f || PivotTranslation.Y != 0.0f || PivotTranslation.Z != 0.0f);

	if (bRotationInLocalSpace)
	{
		if (!bHasPivot)
		{
			const FQuat CurrentQuat = UpdatedComponent->GetRelativeRotation().Quaternion();
			const FQuat NewQuat = CurrentQuat * DeltaQuat;
			UpdatedComponent->SetRelativeRotation(NewQuat.ToFRotator());
		}
		else
		{
			const FVector OldLocation = UpdatedComponent->GetRelativeLocation();
			const FQuat OldRotation = UpdatedComponent->GetRelativeRotation().Quaternion();

			const FVector Pivot = OldLocation + OldRotation.RotateVector(PivotTranslation);
			const FQuat NewRotation = OldRotation * DeltaQuat;
			const FVector NewLocation = Pivot + NewRotation.RotateVector(-PivotTranslation);

			UpdatedComponent->SetRelativeLocation(NewLocation);
			UpdatedComponent->SetRelativeRotation(NewRotation.ToFRotator());
		}
	}
	else
	{
		if (!bHasPivot)
		{
			const FQuat CurrentQuat = UpdatedComponent->GetRelativeRotation().Quaternion();
			const FQuat NewQuat = DeltaQuat * CurrentQuat;
			UpdatedComponent->SetRelativeRotation(NewQuat.ToFRotator());
		}
		else
		{
			const FVector OldLocation = UpdatedComponent->GetRelativeLocation();
			const FQuat OldRotation = UpdatedComponent->GetRelativeRotation().Quaternion();

			const FVector Pivot = OldLocation + PivotTranslation;
			const FQuat NewRotation = DeltaQuat * OldRotation;
			const FVector NewLocation = Pivot + DeltaQuat.RotateVector(OldLocation - Pivot);

			UpdatedComponent->SetRelativeLocation(NewLocation);
			UpdatedComponent->SetRelativeRotation(NewRotation.ToFRotator());
		}
	}
}
