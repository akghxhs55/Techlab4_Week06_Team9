#include "EnginePCH.h"
#include "UExpHeightFogComponent.h"

UExpHeightFogComponent::UExpHeightFogComponent()
{
}

UExpHeightFogComponent::~UExpHeightFogComponent()
{
}

FogData& UExpHeightFogComponent::GetFogData() {

	
    auto m = GetWorldMatrix();

    FVector4 worldLocation = m.GetOrigin();

	FogData& data = FogParam;
	data.FogHeightStart = worldLocation.Z;
    
	return data;
}
