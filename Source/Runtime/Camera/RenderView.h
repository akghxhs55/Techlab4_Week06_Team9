#pragma once

#include "Camera/ViewCamera.h"
#include "Math/Frustum.h"

struct FViewInfo;

struct FRenderView
{
	FMatrix View;
	FMatrix Projection;
	FMatrix ViewProjection;
	FFrustumPlanes Frustum;

	FVector CameraLocation;
	FVector CameraForward;
	FVector CameraRight;
	FVector CameraUp;

	FViewCamera RenderCamera;

	float NearZ = 0.1f;
	bool bIsOrthogonal = false;
	FVector2 ViewSize{ 1.0f, 1.0f };

	static FRenderView Build(const FViewInfo& Info);
	
	FRay Deproject(const FVector2& ScreenPosition) const;

	FMatrix BuildBillboardMatrix(const FVector& WorldPosition, float Width, float Height) const;
};
