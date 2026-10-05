#include "EnginePCH.h"
#include "RenderView.h"

#include "ViewInfo.h"
#include "Math/Frustum.h"

FRenderView FRenderView::Build(const FViewInfo& Info)
{
	FRenderView Result{
		.CameraLocation = Info.Transform.Location,
		.CameraForward = Info.Transform.Rotation.RotateVector({ 1.0f, 0.0f, 0.0f }).Normalized(),
		.CameraRight = Info.Transform.Rotation.RotateVector({ 0.0f, 1.0f, 0.0f }).Normalized(),
		.CameraUp = Info.Transform.Rotation.RotateVector({ 0.0f, 0.0f, 1.0f }).Normalized(),
		.NearZ = Info.Projection.NearClip,
		.bIsOrthogonal = Info.IsOrthographic(),
		.ViewSize = Info.ViewSize,
	};

	FCameraTransform RenderTransform = Info.Transform;
	FCameraProjection RenderProjection = Info.Projection;
	if (Result.bIsOrthogonal)
	{
		const float HalfWidth = RenderProjection.OrthoWidth * 0.5f;
		const float HalfHeight = HalfWidth / Info.GetAspectRatio();
		const float Distance = RenderTransform.Location.Length();

		const float Radius = std::max(RenderProjection.FarClip, 2.0f * Distance + 4.0f * std::sqrt(HalfWidth * HalfWidth + HalfHeight * HalfHeight));
		const FVector Forward = RenderTransform.Rotation.RotateVector({ 1.0f, 0.0f, 0.0f }).Normalized();

		RenderTransform.Location = RenderTransform.Location - Forward * (Radius + RenderProjection.NearClip);
		RenderProjection.FarClip = RenderProjection.NearClip + 2.0f * Radius;
	}
	Result.RenderCamera = { RenderTransform, RenderProjection };

	Result.View = BuildViewMatrix(RenderTransform);
	Result.Projection = BuildProjectionMatrix(RenderProjection, Info.GetAspectRatio());
	Result.ViewProjection = Result.View * Result.Projection;
	Result.Frustum = ExtractFrustumPlanes(Result.ViewProjection);

	return Result;
}

FRay FRenderView::Deproject(const FVector2& ScreenPosition) const
{
	assert(ViewSize.X > 0.0f && ViewSize.Y > 0.0f);
	const float NdcX = 2.0f * ScreenPosition.X / ViewSize.X - 1.0f;
	const float NdcY = 1.0f - 2.0f * ScreenPosition.Y / ViewSize.Y;
	const float AspectRatio = ViewSize.X / ViewSize.Y;

	const FVector Forward = RenderCamera.Transform.Rotation.RotateVector({ 1.0f, 0.0f, 0.0f }).Normalized();
	const FVector Right = RenderCamera.Transform.Rotation.RotateVector({ 0.0f, 1.0f, 0.0f }).Normalized();
	const FVector Up = RenderCamera.Transform.Rotation.RotateVector({ 0.0f, 0.0f, 1.0f }).Normalized();

	if (!bIsOrthogonal)
	{
		const float Tangent = tanf(RenderCamera.Projection.FovDegrees * 0.5f * PI / 180.0f);
		const FVector Direction = (Forward + Right * NdcX * Tangent * AspectRatio + Up * NdcY * Tangent).Normalized();
		return { RenderCamera.Transform.Location, Direction };
	}

	assert(RenderCamera.Projection.OrthoWidth > 0.0f);
	const float Height = RenderCamera.Projection.OrthoWidth / AspectRatio;
	const FVector Origin = RenderCamera.Transform.Location + Forward * RenderCamera.Projection.NearClip
		+ Right * NdcX * 0.5f * RenderCamera.Projection.OrthoWidth
		+ Up * NdcY * 0.5f * Height;

	return { Origin, Forward };
}

FMatrix FRenderView::BuildBillboardMatrix(const FVector& WorldPosition, float Width, float Height) const
{
	const FVector Facing = CameraForward * -1.0f;
	FVector Right;
	FVector Up;
	if (bIsOrthogonal)
	{
		Right = CameraRight;
		Up = CameraUp;
	}
	else
	{
		FVector WorldUp{ 0.0f, 0.0f, 1.0f };
		if (std::fabs(Facing.Dot(WorldUp)) > 0.999f)
			WorldUp = { 0.0f, 1.0f, 0.0f };
		Right = Facing.Cross(WorldUp).Normalized();
		Up = -FVector::Cross(Facing, Right).Normalized();
	}

	FMatrix Result;
	Result.SetIdentity();

	Result.M[0][0] = Facing.X; Result.M[0][1] = Facing.Y; Result.M[0][2] = Facing.Z;
	Result.M[1][0] = Right.X * Width; Result.M[1][1] = Right.Y * Width; Result.M[1][2] = Right.Z * Width;
	Result.M[2][0] = Up.X * Height; Result.M[2][1] = Up.Y * Height; Result.M[2][2] = Up.Z * Height;
	Result.M[3][0] = WorldPosition.X; Result.M[3][1] = WorldPosition.Y; Result.M[3][2] = WorldPosition.Z;

	return Result;
}
