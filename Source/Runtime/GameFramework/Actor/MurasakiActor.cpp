#include "EnginePCH.h"
#include "MurasakiActor.h"
#include "Engine/World.h"
#include <cmath>

AMurasakiActor::AMurasakiActor()
{
	// 에디터에서는 정지해 있고 Play(PIE) 모드에서만 틱이 돌도록 설정
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickInEditor = false;

	// 1. Root Component
	SceneRoot = CreateDefaultSubobject<USceneComponent>("SceneRoot");
	SetRootComponent(SceneRoot);

	// 2. Aka (Red) Sphere + Red PointLight
	AkaComponent = CreateDefaultSubobject<USphereGlowComponent>("AkaComponent");
	AkaComponent->SetupAttachment(SceneRoot);
	AkaComponent->SetColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	AkaComponent->SetIntensity(10.4f);
	AkaComponent->SetRadius(1.0f);
	AkaComponent->SetRadiusFallOff(1.2f);
	AkaComponent->SetRelativeLocation(FVector(-StartPivotDistance, 0.0f, 0.0f));

	AkaPointLight = CreateDefaultSubobject<UPointLightComponent>("AkaPointLight");
	AkaPointLight->SetupAttachment(AkaComponent);
	AkaPointLight->SetLightColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
	AkaPointLight->SetIntensity(PointLightIntensity);
	AkaPointLight->SetAttenuationRadius(PointLightRadius);

	// 3. Ao (Blue) Sphere + Blue PointLight
	AoComponent = CreateDefaultSubobject<USphereGlowComponent>("AoComponent");
	AoComponent->SetupAttachment(SceneRoot);
	AoComponent->SetColor(FVector4(0.0f, 0.0f, 1.0f, 1.0f));
	AoComponent->SetIntensity(10.4f);
	AoComponent->SetRadius(1.0f);
	AoComponent->SetRadiusFallOff(1.2f);
	AoComponent->SetRelativeLocation(FVector(StartPivotDistance, 0.0f, 0.0f));

	AoPointLight = CreateDefaultSubobject<UPointLightComponent>("AoPointLight");
	AoPointLight->SetupAttachment(AoComponent);
	AoPointLight->SetLightColor(FVector4(0.0f, 0.2f, 1.0f, 1.0f));
	AoPointLight->SetIntensity(PointLightIntensity);
	AoPointLight->SetAttenuationRadius(PointLightRadius);

	// 4. Murasaki (Purple) Core Sphere + Giant Purple PointLight
	MurasakiComponent = CreateDefaultSubobject<USphereGlowComponent>("MurasakiComponent");
	MurasakiComponent->SetupAttachment(SceneRoot);
	MurasakiComponent->SetColor(FVector4(0.8f, 0.0f, 1.0f, 1.0f));
	MurasakiComponent->SetIntensity(0.0f);
	MurasakiComponent->SetRadius(1.0f);
	MurasakiComponent->SetRadiusFallOff(1.2f);

	MurasakiPointLight = CreateDefaultSubobject<UPointLightComponent>("MurasakiPointLight");
	MurasakiPointLight->SetupAttachment(SceneRoot);
	MurasakiPointLight->SetLightColor(FVector4(0.8f, 0.0f, 1.0f, 1.0f));
	MurasakiPointLight->SetIntensity(0.0f);
	MurasakiPointLight->SetAttenuationRadius(0.0f);

	// 5. Projectile Movement Component (Play 시에만 동작)
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovementComponent");
	ProjectileMovementComponent->SetInitialSpeed(0.0f);
	ProjectileMovementComponent->SetMaxSpeed(300.0f);
	ProjectileMovementComponent->SetGravityScale(0.0f);
	ProjectileMovementComponent->PrimaryComponentTick.bTickInEditor = false;
}

void AMurasakiActor::BeginPlay()
{
	Super::BeginPlay();
	InitialActorLocation = GetActorLocation();
	ResetSequence();
}

void AMurasakiActor::ResetSequence()
{
	CurrentState = EMurasakiState::Converging;
	ElapsedTime = 0.0f;
	MergeTimer = 0.0f;
	CurrentAngle = 0.0f;

	if (SceneRoot)
	{
		SceneRoot->SetRelativeLocation(InitialActorLocation);
	}

	if (AkaComponent)
	{
		AkaComponent->SetColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
		AkaComponent->SetIntensity(10.4f);
		AkaComponent->SetRadius(1.0f);
		AkaComponent->SetRadiusFallOff(1.2f);
		AkaComponent->SetRelativeLocation(FVector(-StartPivotDistance, 0.0f, 0.0f));
	}
	if (AkaPointLight)
	{
		AkaPointLight->SetLightColor(FVector4(1.0f, 0.0f, 0.0f, 1.0f));
		AkaPointLight->SetIntensity(PointLightIntensity);
		AkaPointLight->SetAttenuationRadius(PointLightRadius);
	}

	if (AoComponent)
	{
		AoComponent->SetColor(FVector4(0.0f, 0.0f, 1.0f, 1.0f));
		AoComponent->SetIntensity(10.4f);
		AoComponent->SetRadius(1.0f);
		AoComponent->SetRadiusFallOff(1.2f);
		AoComponent->SetRelativeLocation(FVector(StartPivotDistance, 0.0f, 0.0f));
	}
	if (AoPointLight)
	{
		AoPointLight->SetLightColor(FVector4(0.0f, 0.2f, 1.0f, 1.0f));
		AoPointLight->SetIntensity(PointLightIntensity);
		AoPointLight->SetAttenuationRadius(PointLightRadius);
	}

	if (MurasakiComponent)
	{
		MurasakiComponent->SetColor(FVector4(0.8f, 0.0f, 1.0f, 1.0f));
		MurasakiComponent->SetIntensity(0.0f);
		MurasakiComponent->SetRadius(1.0f);
		MurasakiComponent->SetRadiusFallOff(1.2f);
	}

	if (MurasakiPointLight)
	{
		MurasakiPointLight->SetLightColor(FVector4(0.8f, 0.0f, 1.0f, 1.0f));
		MurasakiPointLight->SetIntensity(0.0f);
		MurasakiPointLight->SetAttenuationRadius(0.0f);
	}

	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->SetVelocity(FVector::ZeroVector);
	}
}

void AMurasakiActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Play(PIE) 모드일 때만 시퀀스 동작
	if (!GetWorld() || GetWorld()->GetWorldType() != EWorldType::PIE)
	{
		return;
	}

	switch (CurrentState)
	{
	case EMurasakiState::Converging:
	{
		ElapsedTime += DeltaTime;
		const float Duration = (ConvergeDuration > 0.001f) ? ConvergeDuration : 0.001f;
		const float RawProgress = ElapsedTime / Duration;
		const float Progress = (RawProgress > 1.0f) ? 1.0f : ((RawProgress < 0.0f) ? 0.0f : RawProgress);

		// 나선 회전: 중심으로 가까워질수록 회전 각속도 점증 (180 deg/s -> 720 deg/s)
		const float AngularSpeed = 180.0f + 540.0f * Progress;
		CurrentAngle += AngularSpeed * DeltaTime;
		const float Rad = CurrentAngle * (PI / 180.0f);

		// Smoothstep 보간으로 중심에 부드럽게 수렴 (끝날 때 속도가 0으로 부드럽게 감속)
		const float SmoothT = Progress * Progress * (3.0f - 2.0f * Progress);
		const float CurrentDist = (1.0f - SmoothT) * StartPivotDistance;

		const FVector AkaOffset(-CurrentDist * std::cos(Rad), -CurrentDist * std::sin(Rad), 0.0f);
		const FVector AoOffset(+CurrentDist * std::cos(Rad), +CurrentDist * std::sin(Rad), 0.0f);

		if (AkaComponent)
		{
			AkaComponent->SetRelativeLocation(AkaOffset);
		}
		if (AoComponent)
		{
			AoComponent->SetRelativeLocation(AoOffset);
		}

		// 5초 경과하여 0,0에 도달하면 즉시 Merged 상태로 부드럽게 연결
		if (Progress >= 1.0f)
		{
			CurrentState = EMurasakiState::Merged;
			MergeTimer = 0.0f;
			if (AkaComponent) AkaComponent->SetRelativeLocation(FVector::ZeroVector);
			if (AoComponent) AoComponent->SetRelativeLocation(FVector::ZeroVector);
		}
		break;
	}

	case EMurasakiState::Merged:
	{
		MergeTimer += DeltaTime;
		const float BloomDuration = (MergeBloomDuration > 0.001f) ? MergeBloomDuration : 0.001f;
		const float RawAlpha = MergeTimer / BloomDuration;
		const float BloomAlpha = (RawAlpha > 1.0f) ? 1.0f : ((RawAlpha < 0.0f) ? 0.0f : RawAlpha);

		// 3초 동안 서서히 커지는 수치 (10.4 -> 80.0, 1.0 -> 2.5, 1.2 -> 3.5)
		const float CurIntensity = 10.4f + (MurasakiIntensity - 10.4f) * BloomAlpha;
		const float CurRadius = 1.0f + (MurasakiRadius - 1.0f) * BloomAlpha;
		const float CurFallOff = 1.2f + (MurasakiRadiusFallOff - 1.2f) * BloomAlpha;

		if (MurasakiComponent)
		{
			MurasakiComponent->SetColor(FVector4(0.8f, 0.0f, 1.0f, 1.0f));
			MurasakiComponent->SetIntensity(CurIntensity);
			MurasakiComponent->SetRadius(CurRadius);
			MurasakiComponent->SetRadiusFallOff(CurFallOff);
		}

		// 주변 배경을 밝히는 보라색 포인트 라이트도 함께 서서히 강해짐
		if (MurasakiPointLight)
		{
			MurasakiPointLight->SetLightColor(FVector4(0.8f, 0.0f, 1.0f, 1.0f));
			MurasakiPointLight->SetIntensity(180.0f * BloomAlpha);
			MurasakiPointLight->SetAttenuationRadius(60.0f * BloomAlpha);
		}

		// 아카(Red)와 아오(Blue)도 보라색으로 서서히 융합
		const FVector4 RedColor(1.0f, 0.0f, 0.0f, 1.0f);
		const FVector4 BlueColor(0.0f, 0.2f, 1.0f, 1.0f);
		const FVector4 PurpleColor(0.8f, 0.0f, 1.0f, 1.0f);

		auto LerpVec4 = [](const FVector4& A, const FVector4& B, float T) -> FVector4 {
			return FVector4(
				A.X + (B.X - A.X) * T,
				A.Y + (B.Y - A.Y) * T,
				A.Z + (B.Z - A.Z) * T,
				A.W + (B.W - A.W) * T
			);
		};

		const FVector4 CurAkaColor = LerpVec4(RedColor, PurpleColor, BloomAlpha);
		const FVector4 CurAoColor = LerpVec4(BlueColor, PurpleColor, BloomAlpha);

		if (AkaComponent)
		{
			AkaComponent->SetColor(CurAkaColor);
			AkaComponent->SetIntensity(CurIntensity * 0.5f);
		}
		if (AoComponent)
		{
			AoComponent->SetColor(CurAoColor);
			AoComponent->SetIntensity(CurIntensity * 0.5f);
		}
		if (AkaPointLight) AkaPointLight->SetLightColor(CurAkaColor);
		if (AoPointLight) AoPointLight->SetLightColor(CurAoColor);

		// 3초 경과 시 전방으로 발사!
		if (MergeTimer >= BloomDuration)
		{
			CurrentState = EMurasakiState::Launched;
			const FVector ForwardVector = GetActorTransform().GetForward();
			if (ProjectileMovementComponent)
			{
				ProjectileMovementComponent->SetVelocity(ForwardVector * LaunchSpeed);
			}
		}
		break;
	}

	case EMurasakiState::Launched:
	{
		const FVector ForwardVector = GetActorTransform().GetForward();
		if (SceneRoot)
		{
			SceneRoot->SetRelativeLocation(SceneRoot->GetRelativeLocation() + ForwardVector * (LaunchSpeed * DeltaTime));
		}
		break;
	}
	}
}

void AMurasakiActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	SceneRoot = Cast<USceneComponent>(RootComponent);
	for (UActorComponent* Component : GetComponents())
	{
		if (USphereGlowComponent* SGC = Cast<USphereGlowComponent>(Component))
		{
			const FString CompName = SGC->GetName();
			if (CompName.find("Aka") != std::string::npos)
			{
				AkaComponent = SGC;
			}
			else if (CompName.find("Ao") != std::string::npos)
			{
				AoComponent = SGC;
			}
			else
			{
				MurasakiComponent = SGC;
			}
		}
		else if (UPointLightComponent* PLC = Cast<UPointLightComponent>(Component))
		{
			const FString LightName = PLC->GetName();
			if (LightName.find("Aka") != std::string::npos)
			{
				AkaPointLight = PLC;
			}
			else if (LightName.find("Ao") != std::string::npos)
			{
				AoPointLight = PLC;
			}
			else
			{
				MurasakiPointLight = PLC;
			}
		}
		else if (UProjectileMovementComponent* PMC = Cast<UProjectileMovementComponent>(Component))
		{
			ProjectileMovementComponent = PMC;
		}
	}
}
