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

	const FVector OldLocation = UpdatedComponent->GetRelativeLocation();
	const FQuat OldRotation = UpdatedComponent->GetRelativeRotation().Quaternion();

	const FQuat NewRotation = bRotationInLocalSpace ? (OldRotation * DeltaQuat) : (DeltaQuat * OldRotation);

	FVector DeltaLocation = FVector::ZeroVector;
	if (bHasPivot)
	{
		const FVector OldPivot = OldRotation.RotateVector(PivotTranslation);
		const FVector NewPivot = NewRotation.RotateVector(PivotTranslation);
		DeltaLocation = OldPivot - NewPivot;
	}

	UpdatedComponent->SetRelativeLocation(OldLocation + DeltaLocation);
	UpdatedComponent->SetRelativeRotation(NewRotation.ToFRotator());
}
