#pragma once

#include "ActorComponent.h"

class USceneComponent;

class UProjectileMovementComponent : public UActorComponent
{
	DECLARE_CLASS(UProjectileMovementComponent, UActorComponent)

	REFLECT_START(UProjectileMovementComponent)
		PROPERTY(InitialSpeed)
		PROPERTY(MaxSpeed)
		PROPERTY(ProjectileGravityScale)
	REFLECT_END()

public:
	UProjectileMovementComponent();
	virtual ~UProjectileMovementComponent() override = default;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
	virtual void DuplicateSubObjects() override;

	// Speed & Velocity Accessors
	float GetInitialSpeed() const { return InitialSpeed; }
	void SetInitialSpeed(float InSpeed) { InitialSpeed = InSpeed; }

	float GetMaxSpeed() const { return MaxSpeed; }
	void SetMaxSpeed(float InSpeed) { MaxSpeed = InSpeed; }

	float GetGravityScale() const { return ProjectileGravityScale; }
	void SetGravityScale(float InScale) { ProjectileGravityScale = InScale; }

	const FVector& GetVelocity() const { return Velocity; }
	void SetVelocity(const FVector& InVelocity) { Velocity = InVelocity; }

	USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }
	void SetUpdatedComponent(USceneComponent* InComponent) { UpdatedComponent = InComponent; }

protected:
	float InitialSpeed = 10.0f;
	float MaxSpeed = 50.0f;
	float ProjectileGravityScale = 0.0f;

	FVector Velocity = FVector(0.0f, 0.0f, 0.0f);
	USceneComponent* UpdatedComponent = nullptr;
	static constexpr float StandardGravity = 0.0f;
};
