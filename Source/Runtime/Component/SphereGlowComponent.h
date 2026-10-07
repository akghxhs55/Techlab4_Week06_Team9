#pragma once

#include "Component/PrimitiveComponent.h"

struct alignas(16) FSphereGlowConstants
{
    FVector Center;          
    float Radius = 1.0f;    
    FVector4 Color = FVector4(1.0f, 0.0f, 0.0f, 1.0f);           
    float Intensity = 10.4f;
    float RadiusFallOff = 1.2f;
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

    float GetRadius() const { return Radius; }
    void SetRadius(float InRadius) { Radius = InRadius; }

    float GetIntensity() const { return Intensity; }
    void SetIntensity(float InIntensity) { Intensity = InIntensity; }

    float GetRadiusFallOff() const { return RadiusFallOff; }
    void SetRadiusFallOff(float InFallOff) { RadiusFallOff = InFallOff; }

    const FVector4& GetColor() const { return Color; }
    void SetColor(const FVector4& InColor) { Color = InColor; }

protected:
    // 에디터/인스펙터 노출 속성
    float Radius = 1.0f;
    float Intensity = 10.4f;
    float RadiusFallOff = 1.2f;
    FVector4 Color = FVector4(1.0f, 0.0f, 0.0f, 1.0f);

    float LastRadius = -1.0f;

    // GPU 렌더링 전달용 캐시
    FSphereGlowConstants CachedConstants;

    UStaticMesh* SphereGlowMesh = nullptr;
    UMaterial* SphereGlowMaterial = nullptr;
};