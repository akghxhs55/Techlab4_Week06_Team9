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
