#include "EnginePCH.h"
#include "BenchmarkApp.h"

#include "Asset/AssetManager.h"
#include "Core/Window.h"
#include "Core/EngineLog.h"
#include "Core/Stats/LightweightStats.h"
#include "Editor/EditorUI/ImGuiRenderer.h"
#include "Launch/LaunchEngineLoop.h"
#include "Render/Renderer.h"
#include "Render/RenderCommand.h"

#include "GameFramework/Actor/StaticMeshActor.h"

#include "Asset/LOD/StaticMeshLODSelector.h"

#include "Serialization/JsonArchive.h"

#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/Details/DetailsPanel.h"
#include "Editor/EditorControls/EditorControlsPanel.h"
#include "Editor/OutputLog/OutputLogPanel.h"

#include "Input/InputSystem.h"
#include "Collision/HitResult.h"
#include "Core/Stats/EditorStats.h"

namespace
{
	// 숫자 셀 오른쪽 정렬
	void DrawTimeCell(double Milliseconds);
}

DECLARE_CYCLE_STAT("ImGui Render", STAT_ImGuiRender);
// 기존 스탯에 잡히지 않던 구간. 원인을 찾으면 필요 없는 것은 지운다.
DECLARE_CYCLE_STAT("[Tick] Gizmo & Picking Update", STAT_TickGizmoPicking);
DECLARE_CYCLE_STAT("[Tick] World Tick (Total)", STAT_TickWorld);
DECLARE_CYCLE_STAT("[Tick] Editor UI Tick", STAT_TickEditorUI);
DECLARE_CYCLE_STAT("[Tick] Gather (Total)", STAT_TickGather);
DECLARE_CYCLE_STAT("[Tick] Begin Backbuffer Pass", STAT_TickBeginPass);
DECLARE_CYCLE_STAT("[Tick] Sort+Opaque (Total)", STAT_TickSortOpaque);
DECLARE_CYCLE_STAT("[Tick] Grid Render", STAT_TickGrid);
DECLARE_CYCLE_STAT("[Tick] Translucent+Outline+Gizmo", STAT_TickTranslucentOverlay);
DECLARE_CYCLE_STAT("[Tick] End Backbuffer Pass", STAT_TickEndPass);
DECLARE_CYCLE_STAT("[Tick] Stat EndFrame", STAT_TickStatEndFrame);

FEngineConfig UBenchmarkEngine::GetConfig() const
{
	FEngineConfig Desc = Super::GetConfig();
	Desc.Title = L"Benchmarker";
	return Desc;
}

bool UBenchmarkEngine::Init()
{
	if (!Super::Init())
		return false;

	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(GetEngineLoop().GetMainWindow()->GetHandle(), GetEngineLoop().GetRenderDevice()->GetDevice(), GetEngineLoop().GetRenderDevice()->GetContext()))
	{
		return false;
	}

	UStaticMesh* Mesh = UAssetManager::LoadObjStaticMesh("Assets/Data/apple_mid.obj");
	UStaticMesh* Mesh_2 = UAssetManager::LoadObjStaticMesh("Assets/Data/bitten_apple_mid.obj");

	if (!Mesh || !Mesh_2)
		return false;

	FLODGenerateRequest Request;
	// 화면 절반 높이 대비 반지름 비율. 0.06·0.025는 2560x1600 기준 반지름 48px·20px에서 LOD2·LOD3으로 전환한다.
	Request.ScreenThresholds = { 0.25f, 0.06f, 0.025f };
	Request.bSaveToAsset = false; // 현재 저장 경로가 미구현

	auto GenerateFor = [&](UStaticMesh* Asset)
		{
			const FLODGenerateResult Report = UAssetManager::Get().GenerateStaticMeshLODs(*Asset, Request);
			const FString Diagnostics = std::format(
				"[Benchmark] {}: LOD {}. target={}/{}/{}/{}, actual={}/{}/{}/{}, volume={:.3f}/{:.3f}/{:.3f}/{:.3f}, feature={}, topology={}, flips={}, error={}, reason={}\n",
				Asset->GetPath(), Report.bSuccess ? "OK" : "FAILED",
				Report.TargetTriangles[0], Report.TargetTriangles[1],
				Report.TargetTriangles[2], Report.TargetTriangles[3],
				Report.ActualTriangles[0], Report.ActualTriangles[1],
				Report.ActualTriangles[2], Report.ActualTriangles[3],
				Report.VolumeRatio[0], Report.VolumeRatio[1],
				Report.VolumeRatio[2], Report.VolumeRatio[3],
				Report.RejectedFeatureEdges, Report.RejectedTopology,
				Report.RejectedFlips, Report.RejectedError, Report.FailureReason);
			OutputDebugStringA(Diagnostics.c_str());

			if (!Report.bSuccess)
			{
				HTR_LOG(Warning, "LOD generation failed for {}: {}", Asset->GetPath(), Report.FailureReason);
				return;
			}

			HTR_LOG(Info, "{} LOD triangles: {} / {} / {} / {}",
				Asset->GetPath(),
				Report.ActualTriangles[0],
				Report.ActualTriangles[1],
				Report.ActualTriangles[2],
				Report.ActualTriangles[3]);
		};

	GenerateFor(Mesh);
	GenerateFor(Mesh_2);

	FJsonArchive::LoadWorld(World, "Scenes/Default.scene");

	World->GetMainCamera()->GetCameraComponent()->SetRelativeLocation(FVector(-50.0f, 0.0f, 0.0f));

	InitEditorTools();

	return true;
}

// 에디터 패널·기즈모·아웃라인·그리드를 단일 View 기준으로 준비한다.
void UBenchmarkEngine::InitEditorTools()
{
	FRenderer* Renderer = GetEngineLoop().GetRenderer();

	GridRenderer = MakeUnique<FGridRenderer>();
	GridRenderer->Init(Renderer);

	LineBatcher = MakeUnique<FLineBatcher>();
	LineBatcher->Init(Renderer, World);

	Gizmo = MakeUnique<FGizmo>();
	GizmoRenderer = MakeUnique<FGizmoRenderer>();
	GizmoRenderer->Init(Renderer);

	Outline = MakeUnique<FOutline>();
	OutlineRenderer = MakeUnique<FOutlineRenderer>();
	OutlineRenderer->Init(Renderer);

	EditorUI = MakeUnique<FEditorUI>();
	EditorUI->Init(true, true);

	OutputLogPanel = EditorUI->AddEditorPanel<FOutputLogPanel>();
	FLog::AddSink(OutputLogPanel);

	DetailsPanel = EditorUI->AddEditorPanel<FDetailsPanel>();
	DetailsPanel->SetWorld(World);

	EditorControlsPanel = EditorUI->AddEditorPanel<FEditorControlsPanel>();
	EditorControlsPanel->SetWorld(World);
	EditorControlsPanel->SetGizmo(Gizmo.get());

	OutlinerPanel = EditorUI->AddEditorPanel<FOutlinerPanel>();
	OutlinerPanel->SetWorld(World);
	OutlinerPanel->SetSelectionCallback(
		[this](UPrimitiveComponent* Primitive) { SelectPrimitive(Primitive); });
	OutlinerPanel->SetDeleteActorCallback(
		[this](AActor* Actor)
		{
			// 선택된 물체를 지우면 선택 표시(UUID·박스)가 해제된 포인터를 읽지 않도록 먼저 비운다.
			if (UPrimitiveComponent* Selected = GetSelectedPrimitive(); Selected && Selected->GetOwner() == Actor)
				SelectPrimitive(nullptr);
			World->DestroyActor(Actor);
		});

	HTR_LOG(Info, "Benchmark editor tools ready.");
}

// Outliner·Gizmo·Outline·Details의 선택 대상을 한 번에 맞춘다.
void UBenchmarkEngine::SelectPrimitive(UPrimitiveComponent* Primitive)
{
	Gizmo->SetTarget(Primitive);
	Outline->SetTarget(Primitive);
	DetailsPanel->SetTarget(Primitive);
}

UPrimitiveComponent* UBenchmarkEngine::GetSelectedPrimitive() const
{
	return Gizmo ? Cast<UPrimitiveComponent>(Gizmo->GetTarget()) : nullptr;
}

// 선택된 물체 하나의 월드 AABB만 그린다. 컬링·피킹에 실제로 쓰는 프록시 바운드와 같다.
void UBenchmarkEngine::DrawSelectionBounds(const FMatrix& ViewProjection)
{
	UPrimitiveComponent* Selected = GetSelectedPrimitive();
	if (!Selected || !LineBatcher)
		return;

	FBox Bounds;
	if (const FPrimitiveSceneProxy* Proxy = Selected->GetSceneProxy())
	{
		const FAABB& WorldBounds = Proxy->GetBounds();
		Bounds = FBox{ WorldBounds.Center - WorldBounds.Extent, WorldBounds.Center + WorldBounds.Extent };
	}
	else
	{
		Bounds = Selected->CalcBounds();
	}

	LineBatcher->BeginFrame();
	LineBatcher->AddBox(Bounds, FVector4(1.0f, 1.0f, 0.0f, 1.0f));
	LineBatcher->OnRender(ViewProjection);

	// 라인 파이프라인이 바꾼 상태를 장면 기준으로 되돌린다.
	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// 마우스 Ray로 Gizmo를 갱신하고, 축을 잡지 않은 클릭은 피킹으로 처리한다.
void UBenchmarkEngine::UpdateGizmoAndPicking()
{
	// ImGui 패널 위에서의 클릭은 씬 조작으로 넘기지 않는다.
	if (ImGui::GetIO().WantCaptureMouse)
		return;

	const uint32 Width = GetEngineLoop().GetViewportWidth();
	const uint32 Height = GetEngineLoop().GetViewportHeight();
	if (Width == 0 || Height == 0)
		return;

	UCameraComponent* Camera = World->GetMainCamera()->GetCameraComponent();

	const FVector2 MousePosition(
		static_cast<float>(FInputSystem::GetMouseX()),
		static_cast<float>(FInputSystem::GetMouseY()));

	const FRay Ray = Camera->DeProjection(
		MousePosition, static_cast<float>(Width), static_cast<float>(Height));

	Gizmo->Update(
		Ray,
		MousePosition,
		Camera->GetViewProjectionMatrix(),
		static_cast<int>(Width),
		static_cast<int>(Height),
		FInputSystem::IsMouseDown(EMouseButton::Left),
		Camera->GetWorldLocation(),
		Camera->GetIsOrthogonal());

	// 기즈모 축을 집고 있는 중이면 피킹으로 선택을 바꾸지 않는다.
	if (FInputSystem::IsMousePressed(EMouseButton::Left) &&
		!Gizmo->IsUsing() && Gizmo->GetHoveredAxis() < 0)
	{
		
		FHitResult Hit;
		SelectPrimitive(World->LineTraceSingle(Ray, Hit) ? Hit.HitComponent : nullptr);
	}
}

void UBenchmarkEngine::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const uint32 Width = GetEngineLoop().GetViewportWidth();
	const uint32 Height = GetEngineLoop().GetViewportHeight();
	if (Width == 0 || Height == 0)
		return;

	UCameraComponent* Camera = World->GetMainCamera()->GetCameraComponent();
	Camera->SetAspectRatio(static_cast<float>(Width) / Height);

	// 기즈모 조작 결과가 같은 프레임의 UpdateAllTransforms에 반영되도록 World Tick보다 앞에 둔다.
	{
		SCOPE_CYCLE_COUNTER(STAT_TickGizmoPicking);
		UpdateGizmoAndPicking();
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_TickWorld);
		World->Tick(DeltaTime);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_TickEditorUI);
		EditorControlsPanel->DeltaTime = DeltaTime;
		EditorUI->Tick(DeltaTime);
	}

	const FMatrix ViewProjection = Camera->GetViewProjectionMatrix();
	const FMatrix Projection = Camera->GetProjectionMatrix();
	const FFrustumPlanes Frustum = ExtractFrustumPlanes(ViewProjection);


	const float ScaleX = Projection.M[1][0];
	const float ScaleY = Projection.M[2][1];

	// LOD에 카메라 정보 저장
	FLODViewContext LODView{ Width, Height };
	LODView.CameraPosition = Camera->GetWorldLocation();
	LODView.CameraForward = Camera->GetWorldRotation()
		.Quaternion()
		.RotateVector(FVector(1.0f, 0.0f, 0.0f))
		.Normalized();
	LODView.ProjectionScaleSquared = 
		std::max(ScaleX * ScaleX, ScaleY * ScaleY);
	LODView.NearZ = Camera->GetNearZ();
	LODView.bOrthographic = Camera->GetIsOrthogonal();
	LODView.Prepare();

	// 멤버 큐를 재사용한다. Renderer와 swap으로 버퍼를 주고받으므로 두 버퍼 모두 용량이 유지된다.
	{
		SCOPE_CYCLE_COUNTER(STAT_TickGather);
		RenderQueue.Reset();
		World->GatherRenderPackets(RenderQueue, &LODView, &Frustum);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_TickBeginPass);
		GetEngineLoop().BeginBackbufferPass();
	}

	FRenderer* Renderer = GetEngineLoop().GetRenderer();
	const FViewportSettings Viewport{ 0, 0, Width, Height, 0.0f, 1.0f };
	const FVector CameraLocation = Camera->GetWorldLocation();
	const FVector CameraForward = Camera->GetTransform().GetForward();

	// 반투명이 Grid 위에 합성되도록 불투명 → Grid → 반투명 순서로 그린다.
	{
		SCOPE_CYCLE_COUNTER(STAT_TickSortOpaque);
		Renderer->RenderQueueSorting(RenderQueue, ViewProjection);
		Renderer->RenderOpaque(ViewProjection);
	}

	// Grid가 깊이를 쓰기 전, 불투명만 그려진 깊이 버퍼로 측정한다.
	if (bMeasureOcclusionRequested)
	{
		bMeasureOcclusionRequested = false;
		LastOcclusionMeasure = Renderer->MeasureOpaqueOcclusion(ViewProjection);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_TickGrid);
		GridRenderer->OnRenderBatchGrid(
			ViewProjection,
			CameraLocation,
			CameraForward,
			EGridPlane::XY,
			static_cast<float>(GridSettings.GridSpacing),
			true,
			Viewport);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_TickTranslucentOverlay);
		// Grid 파이프라인이 바꾼 상태를 장면 기준으로 되돌린다.
		RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
		RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Renderer->RenderTranslucent(ViewProjection);

		DrawSelectionBounds(ViewProjection);

		if (Outline->GetTarget())
			OutlineRenderer->OnRender(*Outline, ViewProjection, Viewport);

		if (Gizmo->GetTarget())
			GizmoRenderer->OnRender(*Gizmo, ViewProjection, CameraLocation, Camera->GetIsOrthogonal());
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ImGuiRender);
		ImGuiRenderer->Begin();
		EditorUI->OnRender();
		DrawProfileOverlay();
		ImGuiRenderer->End();
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_TickEndPass);
		GetEngineLoop().EndBackbufferPass();
	}

	// EndFrame 자신의 시간은 다음 프레임의 표에 반영된다.
	{
		SCOPE_CYCLE_COUNTER(STAT_TickStatEndFrame);
		FStatRegistry::EndFrame();
	}
}

void UBenchmarkEngine::PreExit()
{
	Super::PreExit();

	if (ImGuiRenderer)
	{
		ImGuiRenderer->Shutdown();
		ImGuiRenderer.reset();
	}
}

void UBenchmarkEngine::DrawProfileOverlay()
{
	const FFrameStats& Stats = GetEngineLoop().GetFrameStats();

	constexpr ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(MainViewport->WorkPos, ImGuiCond_Always);
	if (ImGui::Begin("Profile", nullptr, Flags))
	{
		ImGui::Text("Resolution: %u x %u", GetEngineLoop().GetViewportWidth(), GetEngineLoop().GetViewportHeight());
		ImGui::Text("FPS: %.1f (%.2f ms)", Stats.AverageFPS, Stats.AverageFrameMs);
		ImGui::Text("Frame Time: %.2f ms", Stats.AverageFrameMs);

		const FRenderStats& RS = World->GetRenderStats();
		ImGui::Text("Primitives: %u / %u visible", RS.VisiblePrimitives, RS.TotalPrimitives);
		ImGui::Text("Draw Calls: %u", RS.DrawCalls);
		ImGui::Text("Triangles: %.2f M", RS.Triangles / 1'000'000.0);

		if (ImGui::BeginTable("LODStats", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("LOD");
			ImGui::TableSetupColumn("Objects");
			ImGui::TableSetupColumn("Triangles");
			ImGui::TableSetupColumn("Tri %");
			ImGui::TableHeadersRow();
			for (int i = 0; i < 4; ++i)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::Text("LOD%d", i);
				ImGui::TableSetColumnIndex(1); ImGui::Text("%u", RS.LODCounts[i]);
				ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f M", RS.LODTriangles[i] / 1'000'000.0);
				ImGui::TableSetColumnIndex(3); ImGui::Text("%.1f %%", RS.Triangles ? 100.0 * RS.LODTriangles[i] / RS.Triangles : 0.0);
			}
			ImGui::EndTable();
		}

		ImGui::TextUnformatted("CPU Profile");
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset"))
		{
			FStatRegistry::Reset();
		}

		constexpr ImGuiTableFlags TableFlags =
			ImGuiTableFlags_BordersInnerH |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_SizingFixedFit;

		if (ImGui::BeginTable("CPUStats", 4, TableFlags))
		{
			ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthFixed, 190.0f);
			ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Avg (120f)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableHeadersRow();

			for (const auto& [Name, Data] : FStatRegistry::GetAll())
			{
				if (TStatId{ Name } == EditorStats::STAT_PickingTime)
					continue;

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(Name);

				ImGui::TableSetColumnIndex(1);
				DrawTimeCell(Data.GetLastMs());

				ImGui::TableSetColumnIndex(2);
				DrawTimeCell(Data.GetRecentAverageMs());

				ImGui::TableSetColumnIndex(3);
				DrawTimeCell(Data.GetMaxMs());
			}

			ImGui::EndTable();
		}

		if (const FCycleStatData* PickingData = FStatRegistry::Find(EditorStats::STAT_PickingTime))
		{
			ImGui::Text("Picking Time - Last: %.4f ms, Attempts: %d, Acc.: %.4f ms", PickingData->GetLastMs(), PickingData->CallCount, PickingData->GetTotalMs());
		}

		if (UPrimitiveComponent* Selected = GetSelectedPrimitive())
		{
			if (AActor* Owner = Selected->GetOwner())
				ImGui::Text("Selected: %s (UUID %u)", Owner->GetName().c_str(), Owner->GetUUID());
			else
				ImGui::Text("Selected: %s (UUID %u)", Selected->GetName().c_str(), Selected->GetUUID());
		}
		else
		{
			ImGui::TextUnformatted("Selected: None");
		}

		// 측정 전용: 불투명 물체 중 최종 화면에 픽셀을 남긴 비율 = 오클루전 컬링으로 얻을 수 있는 상한
		if (ImGui::SmallButton("Measure Occlusion"))
		{
			bMeasureOcclusionRequested = true;
		}
		if (const FOcclusionMeasureResult& M = LastOcclusionMeasure; M.bValid)
		{
			const auto Percent = [](uint64 Part, uint64 Whole) { return Whole ? 100.0 * Part / Whole : 0.0; };
			ImGui::Text("Visible Objects: %u / %u (%.1f %%) -> occluded %.1f %%",
				M.VisibleObjects, M.TotalObjects, Percent(M.VisibleObjects, M.TotalObjects),
				100.0 - Percent(M.VisibleObjects, M.TotalObjects));
			ImGui::Text("Visible Draws: %u / %u", M.VisibleDraws, M.TotalDraws);
			ImGui::Text("Visible Triangles: %.2f M / %.2f M (%.1f %%)",
				M.VisibleTriangles / 1'000'000.0, M.TotalTriangles / 1'000'000.0,
				Percent(M.VisibleTriangles, M.TotalTriangles));
			ImGui::Text("Measure cost: %.1f ms", M.ElapsedMs);
		}
	}
	ImGui::End();
	ImGui::PopStyleColor(1);
}

namespace
{
	void DrawTimeCell(double Milliseconds)
	{
		char Text[32];
		snprintf(Text, sizeof(Text), "%.2f ms", Milliseconds);

		const float TextWidth = ImGui::CalcTextSize(Text).x;
		const float AvailableWidth = ImGui::GetContentRegionAvail().x;

		if (TextWidth < AvailableWidth)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (AvailableWidth - TextWidth));
		}

		ImGui::TextUnformatted(Text);
	}
}
