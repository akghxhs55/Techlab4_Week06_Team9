#include "EnginePCH.h"
#include "SphereGlowComponent.h"
#include "Asset/AssetManager.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Engine/Scene.h"

USphereGlowComponent::USphereGlowComponent()
{
	SphereGlowMesh = UAssetManager::GetAssetByPath<UStaticMesh>("Sphere");
	SphereGlowMaterial = UAssetManager::GetAssetByPath<UMaterial>("SphereGlowMaterial");
}

void USphereGlowComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue)
{
    const FPrimitiveSceneProxy* Proxy = GetSceneProxy();
    if (!SphereGlowMesh || !Proxy) return;

    if (CachedConstants.Radius != LastRadius)
    {
        LastRadius = CachedConstants.Radius;
        if (FScene* Scene = Proxy->GetScene())
        {
            Scene->MarkDirty(const_cast<FPrimitiveSceneProxy*>(Proxy));
        }
    }

    CachedConstants.Center = GetWorldLocation();
    CachedConstants.Radius = Radius;
    CachedConstants.Color = Color;
    CachedConstants.Intensity = Intensity;
    CachedConstants.RadiusFallOff = RadiusFallOff;

    FRenderPacket& Packet = RenderQueue.AddDefaulted_GetRef();
    Packet.Proxy = Proxy;
    Packet.Mesh = SphereGlowMesh;
    Packet.Material = SphereGlowMaterial;
    Packet.MaterialParamData = &CachedConstants;
    Packet.MaterialParamDataSize = sizeof(FSphereGlowConstants);
    Packet.StartIndex = 0;
    Packet.IndexCount = static_cast<uint32>(SphereGlowMesh->GetMeshData().Indices.Num());
    Packet.Slot = InvalidObjectSlot;
    Packet.LODIndex = 0;
}

const FStaticMeshData* USphereGlowComponent::GetMeshData() const
{
    return SphereGlowMesh ? &SphereGlowMesh->GetMeshData() : nullptr;
}

FBox USphereGlowComponent::CalcLocalBounds() const
{
    const float R = CachedConstants.Radius;
    return FBox(FVector(-R, -R, -R), FVector(R, R, R));
}
