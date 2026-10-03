#pragma once

#include "Component/PrimitiveComponent.h"

struct alignas(16) FSphereGlowConstants
{
    FVector Center;          
    float Radius = 1.0f;    
    FVector4 Color = FVector4(1.0f, 0.0f, 0.0f, 1.0f);           
    float Intensity = 1.0f;
    float RadiusFallOff = 2.0f;
    float Padding[2] = { 0, 0 };
};

class USphereGlowComponent : public UPrimitiveComponent
{
    DECLARE_CLASS(USphereGlowComponent, UPrimitiveComponent)
    REFLECT_START(USphereGlowComponent)
        PROPERTY(Intensity)
        PROPERTY(Radius)
        PROPERTY(RadiusFallOff)
        PROPERTY_TYPE(Color, Color)
        REFLECT_END()

public:
    USphereGlowComponent();
    virtual ~USphereGlowComponent() override = default;

    void SubmitToRenderQueue(FRenderQueue& RenderQueue) override;
    virtual const FStaticMeshData* GetMeshData() const override;
    virtual FBox CalcLocalBounds() const override;

protected:
    // 에디터/인스펙터 노출 속성
    float Radius = 1.0f;
    float Intensity = 1.0f;
    float RadiusFallOff = 2.0f;
    FVector4 Color = FVector4(1.0f, 0.0f, 0.0f, 1.0f);

    float LastRadius = -1.0f;

    // GPU 렌더링 전달용 캐시
    FSphereGlowConstants CachedConstants;

    UStaticMesh* SphereGlowMesh = nullptr;
    UMaterial* SphereGlowMaterial = nullptr;
};