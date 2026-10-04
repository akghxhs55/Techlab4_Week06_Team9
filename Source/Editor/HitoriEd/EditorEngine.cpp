#include "EnginePCH.h"

#include "Editor/HitoriEd/EditorEngine.h"

#include "Core/EngineStatics.h"
#include "Core/EngineTimer.h"
#include "Launch/LaunchEngineLoop.h"
#include "Core/StatOverlay.h"
#include "Input/InputSystem.h"

#include "ObjectSystem/ObjectFactory.h"

#include "Render/GeometryGenerator.h"

#include "Engine/World.h"
#include "Engine/Level.h"

#include "Render/Renderer.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor/LightActor.h"

#include "Asset/AssetManager.h"
#include "Render/RenderResourceManager.h"

#include "Render/RenderCommand.h"
#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/HitoriEd/EditorFileUtils.h"
#include "UObject/UObjectIterator.h"

#include "Core/EngineLog.h"
#include "Core/Stats/LightweightStats.h"


namespace
{
	DECLARE_CYCLE_STAT("Viewport Update", STAT_ViewportUpdate);
	DECLARE_CYCLE_STAT("World Tick", STAT_WorldTick);
	DECLARE_CYCLE_STAT("Editor Tick", STAT_EditorTick);
	DECLARE_CYCLE_STAT("Capture World", STAT_CaptureWorld);
	DECLARE_CYCLE_STAT("Build Render Queue", STAT_BuildRenderQueue);
	DECLARE_CYCLE_STAT("ImGui", STAT_ImGui);
}

#include "Core/SplashScreen.h"

FEngineConfig UEditorEngine::GetConfig() const
{
	FEngineConfig Desc;
	Desc.Title = L"Hitori Engine";
	Desc.Width = 1920;
	Desc.Height = 1080;
	Desc.bBorderless = false;
	Desc.SyncInterval = 0;
	// View는 각자 깊이 버퍼를 쓰고 백버퍼에는 ImGui만 그린다.
	Desc.bCreateDepthBuffer = false;
	Desc.bExitOnEscape = false;
	// 파일이 없으면 검은 배경에 상태 텍스트만 표시된다.
	Desc.SplashImage = "Resources/Splash.png";
	return Desc;
}

// 렌더 자원·월드·에디터와 MultipleViewports 연결을 초기화한다.
// Device·Window·Swapchain·AssetManager는 FEngineLoop가 먼저 만들어 둔다.
bool UEditorEngine::Init()
{
	if (!Super::Init())
		return false;

	// Create a default world context for the engine
	{
		UWorld* World = FObjectFactory::ConstructObject<UWorld>();
		if (!World || !World->Init()) return false;

		EWorldType WorldType = EWorldType::Editor;
		FWorldContext WorldContext = {
			.World = World,
			.WorldType = WorldType
		};
		World->SetWorldType(WorldType);
		WorldContexts.Add(MakeUnique<FWorldContext>(WorldContext));
		EditorWorldContextRef = WorldContexts.Last().get();
	}

	MainWindow = GetEngineLoop().GetMainWindow();
	MainWindowSC = GetEngineLoop().GetSwapchain();
	Renderer = GetEngineLoop().GetRenderer();
	FRenderDevice* RenderDevice = GetEngineLoop().GetRenderDevice();

	EditorUI = MakeUnique<FEditorUI>();
	EditorUI->Init();

	EditorUI->SetNewSceneCallback([this]() { CreateNewScene(); });
	EditorUI->SetOpenSceneCallback([this]() { OpenScene(); });
	EditorUI->SetSaveSceneCallback([this]() { SaveCurrentScene(); });
	EditorUI->SetSaveSceneAsCallback([this]() { SaveSceneAs(); });

	OutputLogPanel = EditorUI->AddEditorPanel<FOutputLogPanel>();
	FLog::AddSink(OutputLogPanel);
	HTR_LOG(Info, "Editor Initialize...");

	HTR_LOG(Info, "Initialize ImGui...");
	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(MainWindow->GetHandle(), RenderDevice->GetDevice(), RenderDevice->GetContext()))
	{
		HTR_LOG(Error, "Failed To Initialize ImGui!");
	}
	HTR_LOG(Info, "Initialize ImGui Success!");

	GridRenderer = MakeUnique<FGridRenderer>();
	GridRenderer->Init(Renderer);

	GizmoRenderer = MakeUnique<FGizmoRenderer>();
	GizmoRenderer->Init(Renderer);

	Gizmo = MakeUnique<FGizmo>();

	// 필요한 Panel들 추가후 raw pointer 반환(소유권 = EditorUI)
	DetailsPanel = EditorUI->AddEditorPanel<FDetailsPanel>();
	EditorControlsPanel = EditorUI->AddEditorPanel<FEditorControlsPanel>();
	ViewportsPanel = EditorUI->AddEditorPanel<FViewportsPanel>();
	ContentDrawerPanel = EditorUI->AddEditorPanel<FContentDrawerPanel>();

	// OutLine
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer);

	SettingsPanel = EditorUI->AddEditorPanel<FSettingsPanel>();

	Outline = MakeUnique<FOutline>();

	SystemFont = UAssetManager::GetAssetByPath<UFont>("Assets/Fonts/Pretendard.json");

	TextRenderer = MakeUnique<FTextRenderer>();
	TextRenderer->Init();

	ScreenQuadRenderer = MakeUnique<FScreenQuadRenderer>();
	ScreenQuadRenderer->Init();

	// TODO: Iterate WorldContext to set each world
	UWorld* World = WorldContexts[0]->World;

	// 투영 행렬 생성 
	MultipleViewportsAdapter.InitializeFromWorld(*World);
	// 화면 나눔 비율 설정 가져오기
	MultipleViewportsAdapter.SetSplitRatio({
		SettingsPanel->GetSettings().MultipleViewportsHorizontal,
		SettingsPanel->GetSettings().MultipleViewportsVertical });
	// SingleView에 사용할 인덱스 설정
	MultipleViewportsAdapter.SetSingleViewIndex(
		SettingsPanel->GetSettings().MultipleViewportsSingleViewIndex);
	// 뷰포트 레이아웃 설정
	MultipleViewportsAdapter.SetLayoutMode(
		SettingsPanel->GetSettings().bMultipleViewportsSingle
		? ELayoutMode::Single
		: ELayoutMode::QuadSplit);
	World->GetMainCamera()->GetCameraComponent()->SetExternalInputManaged(true);

	/// 삭제 예정
	//SceneManager = EditorUI->AddEditorPanel<FSceneManager>();
	//SceneManager->SetWorld(World);

	OutlinerPanel = EditorUI->AddEditorPanel<FOutlinerPanel>();
	OutlinerPanel->SetWorld(World);
	OutlinerPanel->SetSelectionCallback(
		[this](UPrimitiveComponent* Primitive)
		{
			Gizmo->SetTarget(Primitive);
			Outline->SetTarget(Primitive);
			DetailsPanel->SetTarget(Primitive);
		}
	);

	OutlinerPanel->SetDeleteActorCallback(
		[this](AActor* Actor)
		{
			DeleteActor(Actor);
		}
	);

	LineBatcher = MakeUnique<FLineBatcher>();
	LineBatcher->Init(Renderer, World);

	DetailsPanel->SetWorld(World);

	EditorControlsPanel->SetWorld(World);
	EditorControlsPanel->SetGizmo(Gizmo.get());
	EditorControlsPanel->SetViewportAdapter(&MultipleViewportsAdapter);

	SettingsPanel->SetWorld(World);
	SettingsPanel->SetViewportAdapter(&MultipleViewportsAdapter);
	ViewportsPanel->SetViewportAdapter(&MultipleViewportsAdapter);

	SkyboxRenderer = MakeUnique<FSkyboxRenderer>();
	SkyboxRenderer->Init("Assets/SkySphere/Sky.jpg");

	// TODO: Set World that each viewport is rendering.
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		MultipleViewportsAdapter.SetViewWorld(ViewIndex, *EditorWorldContextRef->World);
	}

	return true;
}

// 프레임 시작·View 상태·월드 갱신 후 View별 오프스크린 렌더와 UI 합성을 진행한다.
// 입력·창 메시지는 FEngineLoop가 먼저 처리하고 Present는 호출 직후에 한다.
void UEditorEngine::Tick(const float DeltaTime)
{
	BeginFrame(DeltaTime);
	UpdateMultipleViewportState(DeltaTime);
	TickWorldAndEditor(DeltaTime);
	RenderMultipleViewports();
	EndFrame();
}

// DeltaTime을 패널에 전달하고 에디터 단축키를 처리한다.
void UEditorEngine::BeginFrame(const float DeltaTime)
{
	FStatOverlay::Tick(DeltaTime);
	EditorControlsPanel->FEditorControlsPanel::DeltaTime = DeltaTime;

	if (!ImGui::GetIO().WantTextInput && FInputSystem::IsKeyPressed(EKeyCode::Delete))
		DeleteActor(OutlinerPanel->GetSelectedActor());
}

// 패널의 Layout·Preset 요청과 입력을 Adapter에 반영한다.
void UEditorEngine::UpdateMultipleViewportState(const float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_ViewportUpdate);

	const FVector2 ViewportSize = ViewportsPanel->GetContentSize();
	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();

	ELayoutMode RequestedLayout{};
	int32 RequestedSingleViewIndex = MultipleViewportsAdapter.GetSingleViewIndex();
	if (ViewportsPanel->ConsumeLayoutRequest(RequestedLayout, RequestedSingleViewIndex))
	{
		if (RequestedLayout == ELayoutMode::Single)
			MultipleViewportsAdapter.SetSingleViewIndex(RequestedSingleViewIndex);
		MultipleViewportsAdapter.SetLayoutMode(RequestedLayout);

		FEditorSettings& Settings = SettingsPanel->GetMutableSettings();
		Settings.bMultipleViewportsSingle = RequestedLayout == ELayoutMode::Single;
		Settings.MultipleViewportsSingleViewIndex = RequestedSingleViewIndex;
	}

	int32 PresetViewIndex = InvalidViewIndex;
	EMultipleViewportsCameraPreset RequestedPreset = EMultipleViewportsCameraPreset::Perspective;
	if (ViewportsPanel->ConsumeCameraPresetRequest(PresetViewIndex, RequestedPreset))
		MultipleViewportsAdapter.ApplyCameraPreset(PresetViewIndex, RequestedPreset);
	MultipleViewportsAdapter.UpdateLayout(ViewportSize, LocalMousePosition);

	const float HorizontalDrag = ViewportsPanel->ConsumeHorizontalDrag();
	const float VerticalDrag = ViewportsPanel->ConsumeVerticalDrag();
	if (HorizontalDrag != 0.0f)
		MultipleViewportsAdapter.ApplySplitterDrag(EDragAxis::Horizontal, HorizontalDrag, ViewportSize);
	if (VerticalDrag != 0.0f)
		MultipleViewportsAdapter.ApplySplitterDrag(EDragAxis::Vertical, VerticalDrag, ViewportSize);
	if (HorizontalDrag != 0.0f || VerticalDrag != 0.0f)
	{
		MultipleViewportsAdapter.UpdateLayout(ViewportSize, LocalMousePosition);
		const FSplitRatio Ratio = MultipleViewportsAdapter.GetSplitRatio();
		SettingsPanel->GetMutableSettings().MultipleViewportsHorizontal = Ratio.Horizontal;
		SettingsPanel->GetMutableSettings().MultipleViewportsVertical = Ratio.Vertical;
	}

	int32 PIEViewIndex = InvalidViewIndex;
	EPIECommand PIECommand = EPIECommand::None;
	if (ViewportsPanel->ConsumePIERequest(PIEViewIndex, PIECommand))
	{
		switch (PIECommand)
		{
		case EPIECommand::Start:
			StartPIE(PIEViewIndex);
			break;
		case EPIECommand::Pause:
			PausePIE(!IsPIEPaused());
			break;
		case EPIECommand::Stop:
			EndPIE(PIEViewIndex);
			break;
		default:
			break;
		}
	}

	MultipleViewportsAdapter.UpdateInput(
		DeltaTime,
		LocalMousePosition,
		SettingsPanel->GetSettings().CameraSpeed,
		SettingsPanel->GetSettings().MouseSensitivity);
	// 겹친 창은 Hover 선택에서 제외하고 우클릭 Capture를 우선한다.
	if (ViewportsPanel->IsHovered() || MultipleViewportsAdapter.GetCapturedViewIndex() != InvalidViewIndex)
		MultipleViewportsAdapter.SetEditorViewIndex(MultipleViewportsAdapter.GetActiveViewIndex());
}

// 월드를 한 번 Tick·Capture한 뒤 에디터와 피킹을 갱신한다.
void UEditorEngine::TickWorldAndEditor(const float DeltaTime)
{
	// 월드 상태는 프레임마다 정확히 한 번 갱신하고 캡처한다.
	{
		SCOPE_CYCLE_COUNTER(STAT_WorldTick);
		for (auto& Context : WorldContexts)
		{
			UWorld* World = Context->World;
			assert(World);

			// Skip ticking PIE worlds if paused
			if (Context->WorldType == EWorldType::PIE && bPIEPaused)
				continue;

			World->Tick(DeltaTime);
		}
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_EditorTick);
		EditorUI->Tick(DeltaTime);
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_CaptureWorld);
		for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
		{
			if (MultipleViewportsAdapter.IsViewActive(ViewIndex))
				MultipleViewportsAdapter.CaptureWorld(ViewIndex);
		}
	}
	UpdateGizmoAndPicking();
}

// 공유 월드 캡처로 활성 View별 렌더 큐를 만들고 렌더한다.
void UEditorEngine::RenderMultipleViewports()
{
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		const bool bActive = MultipleViewportsAdapter.IsViewActive(ViewIndex);
		ViewportsPanel->SetView(ViewIndex, MultipleViewportsAdapter.GetViewRect(ViewIndex), bActive);
		if (!bActive)
			continue;

		{
			SCOPE_CYCLE_COUNTER(STAT_BuildRenderQueue);
			MultipleViewportsAdapter.BuildRenderQueue(ViewIndex, RenderQueue);
		}

		RenderFrame(
			ViewIndex,
			ViewportsPanel->GetRenderingInfo(ViewIndex),
			MultipleViewportsAdapter.GetEngineViewProjection(ViewIndex),
			MultipleViewportsAdapter.GetEngineCameraLocation(ViewIndex),
			MultipleViewportsAdapter.GetEngineCameraForward(ViewIndex),
			RenderQueue);


		// 지금까지 그린 결과를 Screen Quad 로 그리는 과정 추가... 
		// 1. 현재 RT 는 어디에 -> Renderer 에 있다. 
		// 1-1. Renderer 에 Screen Quad 를 그려야 하나?	
		// RT 를 BackBuffer 에 그리는 것이 아니라, 화면 크기와 동일한 Texture 에 그리고, 모든 렌더링이 끝난 이후에 Screen Quad 를 그려서 BackBuffer 에 그린다.


		auto& info = ViewportsPanel->GetRenderingInfo(ViewIndex);
		ScreenQuadRenderer->Render(ViewportsPanel->GetViewRenderTarget(ViewIndex), info, MultipleViewportsAdapter.GetEngineProjectionMatrix(ViewIndex));

		RenderOverlay(
			ViewIndex,
			ViewportsPanel->GetRenderingInfo(ViewIndex),
			MultipleViewportsAdapter.GetEngineViewProjection(ViewIndex),
			MultipleViewportsAdapter.GetEngineCameraLocation(ViewIndex),
			MultipleViewportsAdapter.GetEngineCameraForward(ViewIndex),
			RenderQueue);
	}


	// 여기서 Screen Quad Render 를 수행한다. 
	// Buffer Visualization 이 켜져있는 경우, 대상 버퍼를 Texture 로 바인딩 하고, Screen Quad 를 그린다. 
	// 아닌 경우, 일반 RT 를 Texture 로 바인딩 하고, Screen Quad 를 그린다.



	EMultipleViewportsCameraPreset CameraPresets[4]{};
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
		CameraPresets[ViewIndex] = MultipleViewportsAdapter.GetCameraPreset(ViewIndex);
	ViewportsPanel->SetControlState(
		MultipleViewportsAdapter.GetLayoutMode(),
		MultipleViewportsAdapter.GetSingleViewIndex(),
		CameraPresets,
		bPIEPaused);
}

// 화면을 표시하고 UI 변경 후 View 설정을 보관한다.
void UEditorEngine::EndFrame()
{
	PresentFrame();
	// UI 변경 후 설정을 복사해 종료 시 카메라 수명에 의존하지 않는다.
	SettingsPanel->CaptureViewportSettings();
	// 프로파일러 반영
	FStatRegistry::EndFrame();
}

// 입력 View의 Ray와 피킹으로 Gizmo·공유 선택을 갱신한다.
void UEditorEngine::UpdateGizmoAndPicking()
{
	// Delete는 BeginFrame에서 한 번만 처리하고 여기서는 View 입력만 다룬다.
	const int32 ViewIndex = MultipleViewportsAdapter.GetActiveViewIndex();
	if (ViewIndex == InvalidViewIndex || !ViewportsPanel->IsHovered())
		return;

	// Do not pick gizmo if the current world of the active view is not the editor world.
	{
		const UWorld* ActiveViewWorld = MultipleViewportsAdapter.GetViewWorld(ViewIndex);
		assert(ActiveViewWorld);
		if (ActiveViewWorld->GetWorldType() != EWorldType::Editor)
			return;
	}

	const FVector2 LocalMousePosition = ViewportsPanel->GetLocalMousePosition();
	FRay Ray{};
	if (!MultipleViewportsAdapter.TryGetActiveViewRay(LocalMousePosition, Ray))
		return;

	const FRect& Rect = MultipleViewportsAdapter.GetViewRect(ViewIndex);
	const FVector2 ViewLocalMouse(
		LocalMousePosition.X - Rect.X,
		LocalMousePosition.Y - Rect.Y);
	const FMatrix ViewProjection = MultipleViewportsAdapter.GetEngineViewProjection(ViewIndex);
	bool bMouseDown = FInputSystem::IsMouseDown(EMouseButton::Left);

	Gizmo->Update(
		Ray,
		ViewLocalMouse,
		ViewProjection,
		static_cast<int>(Rect.Width),
		static_cast<int>(Rect.Height),
		bMouseDown,
		MultipleViewportsAdapter.GetEngineCameraLocation(ViewIndex),
		MultipleViewportsAdapter.IsOrthographic(ViewIndex));

	if (FInputSystem::IsMousePressed(EMouseButton::Left) && !Gizmo->IsUsing() && Gizmo->GetHoveredAxis() < 0)
	{
		MultipleViewportsAdapter.PickActiveView(LocalMousePosition);
		MultipleViewportsAdapter.ApplyLastPickToOutliner(*OutlinerPanel);
	}

}

// View 행렬로 Scene·Grid·Gizmo·텍스트·Outline을 렌더한다.
void UEditorEngine::RenderFrame(const int32 ViewIndex, const FRenderingInfo& ViewRenderingInfo, const FMatrix& ViewProjection, const FVector& ViewCameraLocation, const FVector& ViewCameraForward, FRenderQueue& RenderQueue)
{
	RenderCommand::BeginRenderPass(ViewRenderingInfo);

	// Get the world of the current viewport is using.
	UWorld* CurrentWorld = MultipleViewportsAdapter.GetViewWorld(ViewIndex);
	assert(CurrentWorld);

	if (SettingsPanel->GetSettings().bDrawBatchLine)
	{
		// 라인 배처는 매 프레임 한 번만 비우고 한 번만 그린다.
		// 바운딩박스는 그 안에 쌓이는 여러 항목 중 하나일 뿐이다.
		LineBatcher->BeginFrame();

		if (SettingsPanel->GetSettings().bDrawBoundingBox)
		{
			LineBatcher->BuildVertexBuffer(*CurrentWorld);

			CurrentWorld->GetPathTracker().OnRender(LineBatcher.get());
		}

		// 선택된 액터가 라이트면 원뿔을 같이 쌓는다
		if (Gizmo->GetTarget())
		{
			if (ALightActor* LightActor = Cast<ALightActor>(Gizmo->GetTarget()->GetOwner()))
			{
				LightActor->GetSpotLightComponent()->DrawDebug(LineBatcher.get());
			}
		}

		LineBatcher->OnRender(ViewProjection);

	}

	const bool bDrawPrimitives = SettingsPanel->GetSettings().bDrawPrimitives;
	// 삼각형 연결은 유지하고 View별 Fill Mode만 선택한다.
	const ERasterizerState SceneRasterizerState = MultipleViewportsAdapter.IsViewWireframe(ViewIndex)
		? ERasterizerState::Wireframe : ERasterizerState::SolidBack;

	// 렌더 루프 — 반드시 RenderAll보다 먼저
	if (!MultipleViewportsAdapter.IsOrthographic(ViewIndex))
	{
		SkyboxRenderer->OnRender(ViewProjection, ViewCameraLocation);
	}


	if (bDrawPrimitives)
	{
		RenderCommand::SetRasterizerState(SceneRasterizerState);
		RenderCommand::SetBlendState(EBlendState::Opaque);
		RenderCommand::SetDepthStencilState(EDepthStencilState::Default);

		RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// 반투명은 Grid 뒤에 합성되어야 하므로 불투명만 먼저 그린다.
		Renderer->RenderQueueSorting(RenderQueue, ViewProjection);
		Renderer->RenderOpaque(ViewProjection);
		// 장면 Wireframe이 Grid·Gizmo·UI로 전파되지 않도록 복원한다.
		RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	}

	if (bDrawPrimitives)
	{
		// Grid 파이프라인이 바꾼 상태를 장면 기준으로 되돌린 뒤 반투명을 먼 것부터 그린다.
		RenderCommand::SetRasterizerState(SceneRasterizerState);
		RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Renderer->RenderTranslucent(ViewProjection);
		RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	}

	// TextRenderComponent 렌더링
	for (TObjectIterator<UTextRenderComponent> TextComponent; TextComponent; ++TextComponent)
	{
		if (!TextComponent || !TextComponent->GetFont() || !TextComponent->IsVisible())
		{
			continue;
		}

		// Skip if this comopnent is not in current viewport world
		// TODO: Modify TObjectIterator to support filtering by world type or find a better way
		if (TextComponent->GetOwner()->GetWorld() != CurrentWorld)
			continue;

		TextRenderer->OnRender(
			TextComponent->GetText(),
			TextComponent->GetWorldMatrix(),
			TextComponent->GetTextSize(),
			*TextComponent->GetFont(),
			ViewProjection
		);
	}

	// Do not draw Gizmo and Outline if the world type of the current view is PIE
	bool bIsPIEWorld = CurrentWorld->GetWorldType() == EWorldType::PIE;

	// 스텐실 기반이라 선택 대상의 가시성이 꺼져 있어도 외곽선만 그린다.
	if (Outline->GetTarget() && !bIsPIEWorld)
	{
		OutlineRenderer->OnRender(*Outline, ViewProjection, ViewRenderingInfo.ViewportSetting);
	}

	if (Gizmo->GetTarget() && !bIsPIEWorld)
	{
		auto Target = Cast<UPrimitiveComponent>(Gizmo->GetTarget());

		FBox box = Target->CalcBounds();

		GizmoRenderer->OnRender(
			*Gizmo,
			ViewProjection,
			ViewCameraLocation,
			MultipleViewportsAdapter.IsOrthographic(ViewIndex));
	}


	if (SettingsPanel->GetSettings().bShowUUID)
	{

		for (AActor* Actor : CurrentWorld->GetPersistentLevel()->GetActors())
		{
			if (!Actor)
				continue;

			UPrimitiveComponent* Primitive =
				Cast<UPrimitiveComponent>(Actor->GetRootComponent());

			if (!Primitive)
				continue;

			FBox Box =
				Primitive->CalcBounds();

			FVector UUIDLocation;
			UUIDLocation.X = (Box.Min.X + Box.Max.X) * 0.5f;
			UUIDLocation.Y = (Box.Min.Y + Box.Max.Y) * 0.5f;
			UUIDLocation.Z = Box.Max.Z + 0.5f;

			FString Text =
				"UUID : " + std::to_string(Actor->GetUUID());

			TextRenderer->BuildTextMesh(
				Text,
				0.5f,
				*SystemFont
			);

			const FMatrix BillboardWorld = MultipleViewportsAdapter.BuildEngineBillboardMatrix(ViewIndex, UUIDLocation, 1.0f, 1.0f);
			TextRenderer->OnRender(Text, BillboardWorld, 0.5f, *SystemFont, ViewProjection);

		}
	}



	RenderCommand::EndRenderPass(ViewRenderingInfo);
}

void UEditorEngine::RenderOverlay(int32 ViewIndex, const FRenderingInfo& ViewRenderingInfo, const FMatrix& ViewProjection, const FVector& ViewCameraLocation, const FVector& ViewCameraForward, FRenderQueue& RenderQueue) {

	UWorld* CurrentWorld = MultipleViewportsAdapter.GetViewWorld(ViewIndex);
	assert(CurrentWorld);
	bool bIsPIEWorld = CurrentWorld->GetWorldType() == EWorldType::PIE;

	// 직교일 때애는 무조건 그리고, 직교가 아니라면, 깊이 렌더링이 아닐 때 그린다. 
	if (MultipleViewportsAdapter.IsOrthographic(ViewIndex) or ViewRenderingInfo.RenderBufferType != ERenderBuffer::Depth) {


		if (SettingsPanel->GetSettings().bDrawBatchLine)
		{
			const EGridPlane GridPlane = MultipleViewportsAdapter.GetGridPlane(ViewIndex);

			if (SettingsPanel->GetSettings().bDrawPSGrid && !MultipleViewportsAdapter.IsOrthographic(ViewIndex))
			{
				GridRenderer->OnRenderPSGrid(
					ViewProjection, ViewCameraLocation, SettingsPanel->GetSettings(), ViewRenderingInfo.ViewportSetting
				);
			}
			else
			{
				GridRenderer->OnRenderBatchGrid(
					ViewProjection,
					ViewCameraLocation,
					ViewCameraForward,
					GridPlane,
					static_cast<float>(SettingsPanel->GetSettings().GridSpacing),
					!MultipleViewportsAdapter.IsOrthographic(ViewIndex) ||
					MultipleViewportsAdapter.GetCameraPreset(ViewIndex) == EMultipleViewportsCameraPreset::OrthographicView,
					ViewRenderingInfo.ViewportSetting
				);
			}
		}
	}

	if (Outline->GetTarget() && !bIsPIEWorld)
	{
		OutlineRenderer->OnRender(*Outline, ViewProjection, ViewRenderingInfo.ViewportSetting);
	}

	if (Gizmo->GetTarget() && !bIsPIEWorld)
	{
		auto Target = Cast<UPrimitiveComponent>(Gizmo->GetTarget());

		FBox box = Target->CalcBounds();

		GizmoRenderer->OnRender(
			*Gizmo,
			ViewProjection,
			ViewCameraLocation,
			MultipleViewportsAdapter.IsOrthographic(ViewIndex));
	}
}


// View Texture가 포함된 UI를 Swapchain 백버퍼에 합성한다. Present는 FEngineLoop가 한다.
void UEditorEngine::PresentFrame()
{
	// Swapchain 렌더링
	RenderCommand::BeginRenderPass(MainWindowSC->GetRenderingInfo());

	{
		SCOPE_CYCLE_COUNTER(STAT_ImGui);

		ImGuiRenderer->Begin();

		EditorUI->OnRender();

		ImGuiRenderer->End();
	}

	RenderCommand::EndRenderPass(MainWindowSC->GetRenderingInfo());
}

// ImGui를 정리한다. UObject 일괄 삭제와 공용 자원·Device 정리는 FEngineLoop가 이어서 한다.
void UEditorEngine::PreExit()
{
	ImGuiRenderer->Shutdown();
}

// 선택과 Gizmo 참조를 정리한 뒤 Actor를 삭제한다.
void UEditorEngine::DeleteActor(AActor* Actor)
{
	if (!Actor)
		return;

	OutlinerPanel->SelectActor(nullptr);

	Actor->Destroy();
}

// 씬 변경으로 무효화된 에디터의 선택 참조를 모두 해제한다.
void UEditorEngine::ResetSceneSelection()
{
	Gizmo->SetTarget(nullptr);
	Outline->SetTarget(nullptr);
	DetailsPanel->SetTarget(nullptr);
	OutlinerPanel->SelectActor(nullptr);
}

// 새 씬 생성이 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::CreateNewScene()
{
	UWorld* World = EditorWorldContextRef->World;
	if (!FEditorFileUtils::NewScene(World))
		return;

	ResetSceneSelection();
}

// 씬 불러오기가 성공하면 에디터 선택 상태를 초기화한다.
void UEditorEngine::OpenScene()
{
	UWorld* World = EditorWorldContextRef->World;
	if (!FEditorFileUtils::LoadScene(World))
		return;

	ResetSceneSelection();
}

// 공통 파일 유틸리티로 현재 씬을 저장한다.
void UEditorEngine::SaveCurrentScene()
{
	UWorld* World = EditorWorldContextRef->World;
	FEditorFileUtils::SaveScene(World);
}

// 공통 파일 유틸리티로 새 경로에 씬을 저장한다.
void UEditorEngine::SaveSceneAs()
{
	UWorld* World = EditorWorldContextRef->World;
	FEditorFileUtils::SaveSceneAs(World);
}

bool UEditorEngine::StartPIE(int32 ViewIndex)
{
	if (ViewIndex < 0 || ViewIndex >= 4)
	{
		HTR_LOG(Error, "Invalid ViewIndex for StartPIE: {}", ViewIndex);
		return false;
	}

	if (IsPIERunning())
	{
		HTR_LOG(Error, "PIE is already running.");
		return false;
	}

	UWorld* OriginalWorld = MultipleViewportsAdapter.GetViewWorld(ViewIndex);

	assert(OriginalWorld);
	if (OriginalWorld->GetWorldType() != EWorldType::Editor)
	{
		HTR_LOG(Error, "StartPIE can only be called on an Editor world. Current world type: {}", static_cast<int>(OriginalWorld->GetWorldType()));
		return false;
	}

	// Create a new PIE world
	UWorld* PIEWorld = OriginalWorld->Duplicate<UWorld>();
	if (!PIEWorld)
	{
		HTR_LOG(Error, "Failed to create PIE world.");
		return false;
	}

	PIEWorld->SetWorldType(EWorldType::PIE);
	FWorldContext PIEWorldContext = {
		.World = PIEWorld,
		.WorldType = EWorldType::PIE
	};

	WorldContexts.Add(MakeUnique<FWorldContext>(PIEWorldContext));
	PIEWorldContextRef = WorldContexts.Last().get();
	MultipleViewportsAdapter.SetViewWorld(ViewIndex, *PIEWorld);

	// Set UI panels to use the PIE world
	{
		OutlinerPanel->SetWorld(PIEWorld);
	}

	return true;
}

bool UEditorEngine::EndPIE(int32 ViewIndex)
{
	if (!IsPIERunning())
	{
		HTR_LOG(Error, "PIE is not running.");
		return false;
	}

	UWorld* PIEWorld = MultipleViewportsAdapter.GetViewWorld(ViewIndex);

	assert(PIEWorld);
	if (PIEWorld->GetWorldType() != EWorldType::PIE)
	{
		HTR_LOG(Error, "EndPIE can only be called on a PIE world. Current world type: {}", static_cast<int>(PIEWorld->GetWorldType()));
		return false;
	}

	// Find FWorldContext for the PIE world and remove it
	int32 PIEWorldContextIndex = -1;
	for (int32 i = 0; i < WorldContexts.Num(); ++i)
	{
		if (WorldContexts[i].get() == PIEWorldContextRef)
		{
			PIEWorldContextIndex = i;
		}
	}
	if (PIEWorldContextIndex == -1)
	{
		return false;
	}

	WorldContexts.RemoveAt(PIEWorldContextIndex, 1);
	MultipleViewportsAdapter.SetViewWorld(ViewIndex, *EditorWorldContextRef->World);
	PIEWorldContextRef = nullptr;

	// Reset UI panels to use the Editor world
	{
		OutlinerPanel->SetWorld(EditorWorldContextRef->World);
	}

	return true;
}
