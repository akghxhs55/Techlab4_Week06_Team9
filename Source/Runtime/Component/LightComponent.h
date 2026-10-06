#pragma once

#include "SceneComponent.h"

class ULightComponent : public USceneComponent
{
	DECLARE_CLASS(ULightComponent, USceneComponent)
	REFLECT_START(ULightComponent)
		PROPERTY(bVisible)
	REFLECT_END()
public:
	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible) { bVisible = bInVisible; }

protected:
	bool bVisible = true;
};