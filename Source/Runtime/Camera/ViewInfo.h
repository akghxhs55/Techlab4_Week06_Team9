#pragma once

#include "Camera/ViewCamera.h"

struct FViewInfo
{
	FCameraTransform Transform;
	FCameraProjection Projection;
	FVector2 ViewSize{ 1.0f, 1.0f };

	float GetAspectRatio() const { return ViewSize.Y > 0.0f ? ViewSize.X / ViewSize.Y : 1.0f; }
	bool IsOrthographic() const { return Projection.Mode == EProjectionMode::Orthographic; }
};
