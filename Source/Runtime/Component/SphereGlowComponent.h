#pragma once

#include "Component/PrimitiveComponent.h"

struct alignas(16) FSphereGlowConstants
{
    FVector Center;          
    float Radius = 100.0f;    
    FVector4 Color = FVector4(1.0f, 0.5f, 0.1f, 1.0f);           
    float Intensity = 1.0f;
    float RadiusFallOff = 2.0f;
    float Padding[2] = { 0, 0 };
};

class USphereGlowComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(USphereGlowComponent, UPrimitiveComponent)
    REFLECT_START(USphereGlowComponent)
        PROPERTY(CachedConstants.Intensity)
        PROPERTY(CachedConstants.Radius)
        PROPERTY(CachedConstants.RadiusFallOff)
        PROPERTY_TYPE(CachedConstants.Color, Color)
    REFLECT_END()

public:
	USphereGlowComponent();
    virtual ~USphereGlowComponent() override = default;

	void SubmitToRenderQueue(FRenderQueue& RenderQueue) override;
	virtual const FStaticMeshData* GetMeshData() const override;
	virtual FBox CalcLocalBounds() const override;

protected:
    FSphereGlowConstants CachedConstants;
    float LastRadius = -1.0f;

	UStaticMesh* SphereGlowMesh = nullptr;
    UMaterial* SphereGlowMaterial = nullptr;
};

