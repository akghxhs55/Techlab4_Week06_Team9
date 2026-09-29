#include "EnginePCH.h"
#include "Scene.h"

void FScene::AddPrimitive(UPrimitiveComponent* Component)
{
	if (!Component || Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = new FPrimitiveSceneProxy(Component);
	Proxy->Scene = this;
	Proxy->PackedIndex = Proxies.Num();
	Component->SceneProxy = Proxy;

	Proxies.Add(Proxy);
	PrimitiveBounds.Add(FAABB{});
	PrimitiveFlags.Add(0);

	bElementListChanged = true;
}

void FScene::RemovePrimitive(UPrimitiveComponent* Component)
{
	if (!Component || !Component->SceneProxy) return;

	FPrimitiveSceneProxy* Proxy = Component->SceneProxy;
	if (!Proxy) return;

	const uint32 Index = static_cast<uint32>(Proxy->PackedIndex);

	Proxies.RemoveAtSwap(Index);
	PrimitiveBounds.RemoveAtSwap(Index);
	PrimitiveFlags.RemoveAtSwap(Index);

	if (Index < static_cast<uint32>(Proxies.Num()))
		Proxies[Index]->PackedIndex = static_cast<int32>(Index);

	Component->SceneProxy = nullptr;
	delete Proxy;

	bElementListChanged = true;
}

void FScene::UpdateAllTransforms()
{
	bool bBoundsChanged = false;

	const int32 Count = Proxies.Num();
	for (int32 i = 0; i < Count; ++i)
	{
		FPrimitiveSceneProxy* Proxy = Proxies[i];
		Proxy->UpdateTransform();

		const FAABB& Bounds = Proxy->GetBounds();
		if (PrimitiveBounds[i].Center != Bounds.Center || PrimitiveBounds[i].Extent != Bounds.Extent)
		{
			bBoundsChanged = true;
		}

		PrimitiveBounds[i] = Proxy->GetBounds();
		PrimitiveFlags[i] = Proxy->GetComponent()->IsVisible() ? 1 : 0;
	}

	if (bElementListChanged)
	{
		BuildBVH();
		bElementListChanged = false;
	}
	else if (bBoundsChanged)
	{
		BVH.Refit();
	}
}

void FScene::BuildBVH()
{
	BVH.Clear();
	// BillboardComponents.Reset();

	TArray<UPrimitiveComponent*> Components;
	for (int32 i = 0; i < Proxies.Num(); ++i)
	{
		if (Proxies[i]->GetComponent())
			Components.Add(Proxies[i]->GetComponent());
	}

	BVH.Build(std::span(Components.GetData(), Components.Num()));
}
