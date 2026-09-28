#include "EnginePCH.h"
#include "BenchmarkApp.h"

#include "Asset/AssetManager.h"
#include "Core/Window.h"
#include "Launch/LaunchEngineLoop.h"
#include "Render/Renderer.h"

#include "GameFramework/Actor/StaticMeshActor.h"

FEngineConfig UBenchmarkEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"Benchmarker";
	return Desc;
}

bool UBenchmarkEngine::Init()
{
	if (!Super::Init())
		return false;

	UStaticMesh* Mesh = UAssetManager::LoadObjStaticMesh("Assets/Models/Apple/apple_mid.obj");   // 실제 경로 확인 필요
	UStaticMesh* Mesh_2 = UAssetManager::LoadObjStaticMesh("Assets/Models/Apple/bitten_apple_mid.obj");   // 실제 경로 확인 필요

	constexpr int32 CountX = 50, CountY = 50, CountZ = 20;
	constexpr float Spacing = 1.0f;
	for (int32 Z = 0; Z < CountZ; ++Z)
		for (int32 Y = 0; Y < CountY; ++Y)
			for (int32 X = 0; X < CountX; ++X)
			{
				FTransform T = FTransform::Identity;
				T.Location = FVector(X * Spacing, Y * Spacing, Z * Spacing);

				AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(NAME_None, &T);
				Actor->GetStaticMeshComponent()->SetStaticMesh(Y % 2 == 0 ? Mesh : Mesh_2);
			}

	return true;
}

void UBenchmarkEngine::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UCameraComponent* Camera = World->GetMainCamera()->GetCameraComponent();
	Camera->SetAspectRatio(static_cast<float>(GetEngineLoop().GetViewportWidth()) / GetEngineLoop().GetViewportHeight());

	TQueue<FRenderPacket> RenderQueue;
	World->GatherRenderPackets(RenderQueue);

	GetEngineLoop().BeginBackbufferPass();
	GetEngineLoop().GetRenderer()->RenderAll(RenderQueue, Camera->GetViewProjectionMatrix());
	GetEngineLoop().EndBackbufferPass();
}
