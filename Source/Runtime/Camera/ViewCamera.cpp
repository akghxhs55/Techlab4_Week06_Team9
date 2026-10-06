#include "EnginePCH.h"
#include "ViewCamera.h"

#include "ViewMath.h"
#include "Camera/ViewInfo.h"
#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Math/Quat.h"
#include "Math/Matrix.h"
#include "Collision/Ray.h"

FViewInfo FViewCamera::ToViewInfo(const FVector2& ViewSize) const
{
	return {
		.Transform = Transform,
		.Projection = Projection,
		.ViewSize = ViewSize
	};
}

// 회전된 카메라 기저와 위치 내적으로 View 행렬을 구성한다.
FMatrix BuildViewMatrix(const FCameraTransform& Transform)
{
    const FVector Forward = ViewMath::Rotate(Transform.Rotation, { 1.0f, 0.0f, 0.0f });
    const FVector Right = ViewMath::Rotate(Transform.Rotation, { 0.0f, 1.0f, 0.0f });
    const FVector Up = ViewMath::Rotate(Transform.Rotation, { 0.0f, 0.0f, 1.0f });
    FMatrix Result = ViewMath::ZeroMatrix();
    Result.M[0][0] = Forward.X; Result.M[1][0] = Forward.Y; Result.M[2][0] = Forward.Z; Result.M[3][0] = -ViewMath::Dot(Forward, Transform.Location);
    Result.M[0][1] = Right.X; Result.M[1][1] = Right.Y; Result.M[2][1] = Right.Z; Result.M[3][1] = -ViewMath::Dot(Right, Transform.Location);
    Result.M[0][2] = Up.X; Result.M[1][2] = Up.Y; Result.M[2][2] = Up.Z; Result.M[3][2] = -ViewMath::Dot(Up, Transform.Location);
    Result.M[3][3] = 1.0f;
    return Result;
}

// 원근은 FOV, 직교는 전체 폭을 기준으로 Projection 행렬을 구성한다.
FMatrix BuildProjectionMatrix(const FCameraProjection& Projection, const float AspectRatio)
{
    assert(AspectRatio > 0.0f);
    assert(Projection.NearClip > 0.0f && Projection.FarClip > Projection.NearClip);
    FMatrix Result = ViewMath::ZeroMatrix();
    if (Projection.Mode == EProjectionMode::Perspective)
    {
        assert(Projection.FovDegrees > 0.0f && Projection.FovDegrees < 180.0f);
        const float ScaleY = 1.0f / tanf(Projection.FovDegrees * ViewMath::Pi / 360.0f);
        Result.M[1][0] = ScaleY / AspectRatio;
        Result.M[2][1] = ScaleY;
        Result.M[0][2] = Projection.FarClip / (Projection.FarClip - Projection.NearClip);
        Result.M[3][2] = -Projection.NearClip * Projection.FarClip / (Projection.FarClip - Projection.NearClip);
        Result.M[0][3] = 1.0f;
    }
    else
    {
        assert(Projection.OrthoWidth > 0.0f);
        const float Height = Projection.OrthoWidth / AspectRatio;
        Result.M[1][0] = 2.0f / Projection.OrthoWidth;
        Result.M[2][1] = 2.0f / Height;
        Result.M[0][2] = 1.0f / (Projection.FarClip - Projection.NearClip);
        Result.M[3][2] = -Projection.NearClip / (Projection.FarClip - Projection.NearClip);
        Result.M[3][3] = 1.0f;
    }
    return Result;
}

// 로컬 이동을 월드로 회전하고 Yaw 뒤 현재 Right축 Pitch를 합성한다.
FViewCamera ApplyCameraMovement(const FViewCamera& Current, const FCameraMoveInput& Input, const float DeltaTime)
{
    assert(DeltaTime >= 0.0f);
    FViewCamera Result = Current;
    const FVector LocalTranslation{ Input.MoveAxis.X + Input.ZoomDelta, Input.MoveAxis.Y, Input.MoveAxis.Z };
    Result.Transform.Location = ViewMath::Add(Result.Transform.Location, ViewMath::Scale(ViewMath::Rotate(Current.Transform.Rotation, LocalTranslation), DeltaTime));

    if (Input.MouseDelta.X != 0.0f || Input.MouseDelta.Y != 0.0f)
    {
        const FQuat Yaw = ViewMath::AxisAngle({ 0.0f, 0.0f, 1.0f }, Input.MouseDelta.X * DeltaTime * ViewMath::Pi / 180.0f);
        const FVector CurrentRight = ViewMath::Rotate(Current.Transform.Rotation, { 0.0f, 1.0f, 0.0f });
        const FQuat Pitch = ViewMath::AxisAngle(CurrentRight, -Input.MouseDelta.Y * DeltaTime * ViewMath::Pi / 180.0f);
        Result.Transform.Rotation = ViewMath::Normalize(ViewMath::Multiply(Pitch, ViewMath::Multiply(Yaw, Current.Transform.Rotation)));
    }
    return Result;
}
