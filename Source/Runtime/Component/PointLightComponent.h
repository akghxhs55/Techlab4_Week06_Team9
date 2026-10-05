#pragma once

#include "LightComponent.h"

class FLineBatcher;

class UPointLightComponent : public ULightComponent
{
	DECLARE_CLASS(UPointLightComponent, ULightComponent)

	REFLECT_START(ClassName)
		PROPERTY(AttenuationRadius)
		PROPERTY(Falloff)
		PROPERTY(Intensity)
		PROPERTY_TYPE(LightColor, Color)
	REFLECT_END()

public:
	UPointLightComponent() = default;
	virtual ~UPointLightComponent() override = default;

	void DrawDebug(FLineBatcher* LineBatcher) const;

	float GetAttenuationRadius() const { return AttenuationRadius; }
	void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }

	float GetFalloff() const { return Falloff; }
	void SetFalloff(float InFalloff) { Falloff = InFalloff; }

	float GetIntensity() const { return Intensity; }
	void SetIntensity(float InIntensity) { Intensity = InIntensity; }

	const FVector4& GetLightColor() const { return LightColor; }
	void SetLightColor(const FVector4& InColor) { LightColor = InColor; }

private:
	float AttenuationRadius = 10.0f;
	float Falloff = 2.0f;
	float Intensity = 1.0f;
	FVector4 LightColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
};
