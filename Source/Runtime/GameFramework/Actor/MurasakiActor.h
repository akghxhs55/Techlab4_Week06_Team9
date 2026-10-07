#pragma once

#include "GameFramework/Actor.h"
#include "Component/SphereGlowComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/ProjectileMovementComponent.h"

enum class EMurasakiState : uint8
{
	Converging,  // 0,0을 향해 나선 수렴 (5초)
	Merged,      // 0,0에 도달하여 보라색으로 융합 및 서서히 거대화 (3초)
	Launched     // 무라사키 완성 후 전방으로 고속 발사
};

class AMurasakiActor : public AActor
{
	DECLARE_CLASS(AMurasakiActor, AActor)

	REFLECT_START(AMurasakiActor)
		PROPERTY(StartPivotDistance)
		PROPERTY(ConvergeDuration)
		PROPERTY(MergeBloomDuration)
		PROPERTY(LaunchSpeed)
		PROPERTY(MurasakiIntensity)
		PROPERTY(MurasakiRadius)
		PROPERTY(MurasakiRadiusFallOff)
		PROPERTY(PointLightIntensity)
		PROPERTY(PointLightRadius)
	REFLECT_END()

public:
	AMurasakiActor();
	virtual ~AMurasakiActor() override = default;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void DuplicateSubObjects() override;

	void ResetSequence();

	USphereGlowComponent* GetAkaComponent() const { return AkaComponent; }
	USphereGlowComponent* GetAoComponent() const { return AoComponent; }
	USphereGlowComponent* GetMurasakiComponent() const { return MurasakiComponent; }
	UPointLightComponent* GetAkaPointLight() const { return AkaPointLight; }
	UPointLightComponent* GetAoPointLight() const { return AoPointLight; }
	UPointLightComponent* GetMurasakiPointLight() const { return MurasakiPointLight; }
	UProjectileMovementComponent* GetProjectileMovementComponent() const { return ProjectileMovementComponent; }

protected:
	// 컴포넌트 구성
	USceneComponent* SceneRoot = nullptr;

	USphereGlowComponent* AkaComponent = nullptr;
	UPointLightComponent* AkaPointLight = nullptr;

	USphereGlowComponent* AoComponent = nullptr;
	UPointLightComponent* AoPointLight = nullptr;

	USphereGlowComponent* MurasakiComponent = nullptr;
	UPointLightComponent* MurasakiPointLight = nullptr;

	UProjectileMovementComponent* ProjectileMovementComponent = nullptr;

	// 인스펙터 조작 속성
	float StartPivotDistance = 10.0f;
	float ConvergeDuration = 5.0f;     // 맨 처음 구현 기준: 5초간 나선 수렴
	float MergeBloomDuration = 3.0f;   // 3초간 서서히 거대화 & 융합
	float LaunchSpeed = 85.0f;         // 초속 85.0+ 고속 돌진
	float MurasakiIntensity = 80.0f;
	float MurasakiRadius = 2.5f;
	float MurasakiRadiusFallOff = 3.5f;
	float PointLightIntensity = 85.0f;
	float PointLightRadius = 30.0f;

	// 런타임 상태
	EMurasakiState CurrentState = EMurasakiState::Converging;
	float ElapsedTime = 0.0f;
	float MergeTimer = 0.0f;
	float CurrentAngle = 0.0f;
	FVector InitialActorLocation = FVector::ZeroVector;
};
