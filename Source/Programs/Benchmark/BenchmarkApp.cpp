#include "EnginePCH.h"
#include "BenchmarkApp.h"

#include "Asset/AssetManager.h"
#include "Core/Window.h"
#include "Core/Stats/LightweightStats.h"
#include "Editor/EditorUI/ImGuiRenderer.h"
#include "Launch/LaunchEngineLoop.h"
#include "Render/Renderer.h"

#include "GameFramework/Actor/StaticMeshActor.h"

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
	ImGuiRenderer = MakeUnique<FImGuiRenderer>();
	if (!ImGuiRenderer->Init(GetEngineLoop().GetMainWindow()->GetHandle(), GetEngineLoop().GetRenderDevice()->GetDevice(), GetEngineLoop().GetRenderDevice()->GetContext()))
	{
		return false;
	}

	World = FObjectFactory::ConstructObject<UWorld>();
	World->Init();


	return true;
}

void UBenchmarkEngine::Tick(float DeltaTime)
{
	FStatRegistry::BeginFrame();
	{
		SCOPE_CYCLE_COUNTER(STAT_WorldTick);
		World->Tick(DeltaTime);
	}

	UCameraComponent* Camera = World->GetMainCamera()->GetCameraComponent();
	Camera->SetAspectRatio(static_cast<float>(GetEngineLoop().GetViewportWidth()) / GetEngineLoop().GetViewportHeight());

	TQueue<FRenderPacket> RenderQueue;
	{
		SCOPE_CYCLE_COUNTER(STAT_GatherRenderPackets);
		World->GatherRenderPackets(RenderQueue);
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
