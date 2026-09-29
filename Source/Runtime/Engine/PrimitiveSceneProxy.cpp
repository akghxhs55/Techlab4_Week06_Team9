#include "EnginePCH.h"
#include "PrimitiveSceneProxy.h"

#include "Component/PrimitiveComponent.h"

void FPrimitiveSceneProxy::UpdateTransform()
{
	LocalToWorld = Component->GetWorldMatrix();
	Bounds = MakeWorldBounds(Component->CalcLocalBounds().GetWorldAABB(LocalToWorld));
}
