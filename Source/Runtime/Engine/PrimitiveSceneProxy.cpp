#include "EnginePCH.h"
#include "PrimitiveSceneProxy.h"

#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Render/Mesh.h"

void FPrimitiveSceneProxy::UpdateTransform()
{
	LocalToWorld = Component->GetWorldMatrix();
	WorldToLocal = LocalToWorld.Inverse();
	Bounds = MakeWorldBounds(Component->CalcLocalBounds().GetWorldAABB(LocalToWorld));
}

void FPrimitiveSceneProxy::UpdateRenderState()
{
	Mesh = nullptr;
	Sections.Reset();
	bVisible = Component->IsVisible();

	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
	if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh()) return;

	Mesh = StaticMeshComponent->GetStaticMesh();
	LODCount = (uint8)std::min<uint32>(Mesh->GetLODCount(), 4);
	for (int i = 0; i < 3; i++)
	{
		LODThresholdSq[i] = Mesh->ScreenThresholds[i] * Mesh->ScreenThresholds[i];
	}

	for (uint32 LOD = 0; LOD < LODCount; ++LOD)
	{
		LODs[LOD].FirstSection = Sections.Num();
		for (const FStaticMeshSection& Section : Mesh->GetMeshData(LOD).Sections)
		{
			UMaterial* Mat = StaticMeshComponent->GetMaterial((int32)Section.MaterialSlotIndex);

			if (Mat) Sections.Add({ Mat, Section.StartIndex, Section.IndexCount });
		}
		LODs[LOD].NumSections = Sections.Num() - LODs[LOD].FirstSection;
	}
}
