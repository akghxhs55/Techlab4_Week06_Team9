#include "EnginePCH.h"
#include "BenchmarkApp.h"

#include "Asset/AssetManager.h"
#include "Core/Window.h"
#include "Core/Stats/LightweightStats.h"
#include "Editor/EditorUI/ImGuiRenderer.h"
#include "Launch/LaunchEngineLoop.h"
#include "Render/Renderer.h"

#include "GameFramework/Actor/StaticMeshActor.h"

#include "Asset/LOD/StaticMeshLODSelector.h"

#include "Serialization/JsonArchive.h"

namespace
{
	// 숫자 셀 오른쪽 정렬
	void DrawTimeCell(double Milliseconds);
}

DECLARE_CYCLE_STAT("World Tick", STAT_WorldTick);
DECLARE_CYCLE_STAT("Gather Render Packets", STAT_GatherRenderPackets);
DECLARE_CYCLE_STAT("ImGui Render", STAT_ImGuiRender);

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
	Request.ScreenThresholds = { 0.15f, 0.07f, 0.01f };
	Request.bSaveToAsset = false; // 현재 저장 경로가 미구현

	auto GenerateFor = [&](UStaticMesh* Asset)
		{
			const FLODGenerateResult Report = UAssetManager::Get().GenerateStaticMeshLODs(*Asset, Request);
			const FString Diagnostics = std::format(
				"[Benchmark] {}: LOD {}. target={}/{}/{}/{}, actual={}/{}/{}/{}, feature={}, topology={}, flips={}, reason={}\n",
				Asset->GetPath(), Report.bSuccess ? "OK" : "FAILED",
				Report.TargetTriangles[0], Report.TargetTriangles[1],
				Report.TargetTriangles[2], Report.TargetTriangles[3],
				Report.ActualTriangles[0], Report.ActualTriangles[1],
				Report.ActualTriangles[2], Report.ActualTriangles[3],
				Report.RejectedFeatureEdges, Report.RejectedTopology,
				Report.RejectedFlips, Report.FailureReason);
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

	return true;
}

void UBenchmarkEngine::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FStatRegistry::BeginFrame();
	{
		SCOPE_CYCLE_COUNTER(STAT_WorldTick);
		World->Tick(DeltaTime);
	}

	const uint32 Width = GetEngineLoop().GetViewportWidth();
	const uint32 Height = GetEngineLoop().GetViewportHeight();
	if (Width == 0 || Height == 0)
		return;

	UCameraComponent* Camera = World->GetMainCamera()->GetCameraComponent();
	Camera->SetAspectRatio(static_cast<float>(Width) / Height);
	const FLODViewContext LODView{ Camera->GetViewProjectionMatrix(), Width, Height };

	TQueue<FRenderPacket> RenderQueue;
	{
		SCOPE_CYCLE_COUNTER(STAT_GatherRenderPackets);
		World->GatherRenderPackets(RenderQueue, &LODView);
	}

	GetEngineLoop().BeginBackbufferPass();

	GetEngineLoop().GetRenderer()->RenderAll(RenderQueue, Camera->GetViewProjectionMatrix());

	{
		SCOPE_CYCLE_COUNTER(STAT_ImGuiRender);
		ImGuiRenderer->Begin();
		DrawProfileOverlay();
		ImGuiRenderer->End();
	}

	GetEngineLoop().EndBackbufferPass();
}

void UBenchmarkEngine::PreExit()
{
	ImGuiRenderer->Shutdown();
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
	if (ImGui::Begin("Profile", nullptr, Flags))
	{
		ImGui::Text("FPS: %.1f", Stats.AverageFPS);
		ImGui::Text("Frame Time: %.2f ms", Stats.AverageFrameMs);

		ImGui::TextUnformatted("CPU Profile");
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset History"))
		{
			FStatRegistry::ResetHistory();
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

			const auto& LastFrameStats = FStatRegistry::GetLastFrameStats();
			for (const auto& [Name, History] : FStatRegistry::GetHistories())
			{
				const auto* Value = LastFrameStats.FindOrNull(Name);

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(Name);

				ImGui::TableSetColumnIndex(1);
				DrawTimeCell(Value ? Value->GetTotalMs() : 0.0);

				ImGui::TableSetColumnIndex(2);
				DrawTimeCell(History.GetRecentAverageMs());

				ImGui::TableSetColumnIndex(3);
				DrawTimeCell(History.GetMaxMs());
			}

			ImGui::EndTable();
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
