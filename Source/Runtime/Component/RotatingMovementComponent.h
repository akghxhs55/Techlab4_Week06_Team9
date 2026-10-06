#pragma once

#include "ActorComponent.h"
#include "Math/Rotator.h"

class USceneComponent;

class URotatingMovementComponent : public UActorComponent
{
	DECLARE_CLASS(URotatingMovementComponent, UActorComponent)

	REFLECT_START(URotatingMovementComponent)
		PROPERTY(RotationRate)
		PROPERTY(PivotTranslation)
		PROPERTY(bRotationInLocalSpace)
	REFLECT_END()

public:
	URotatingMovementComponent();
	virtual ~URotatingMovementComponent() override = default;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
	virtual void DuplicateSubObjects() override;

	const FRotator& GetRotationRate() const { return RotationRate; }
	void SetRotationRate(const FRotator& InRate) { RotationRate = InRate; }

	const FVector& GetPivotTranslation() const { return PivotTranslation; }
	void SetPivotTranslation(const FVector& InPivot) { PivotTranslation = InPivot; }

	bool IsRotationInLocalSpace() const { return bRotationInLocalSpace; }
	void SetRotationInLocalSpace(bool bInLocalSpace) { bRotationInLocalSpace = bInLocalSpace; }

	USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }
	void SetUpdatedComponent(USceneComponent* InComponent) { UpdatedComponent = InComponent; }

protected:
	FRotator RotationRate = FRotator(0.0f, 90.0f, 0.0f);
	FVector PivotTranslation = FVector(0.0f, 0.0f, 0.0f);
	bool bRotationInLocalSpace = true;

	USceneComponent* UpdatedComponent = nullptr;
};
