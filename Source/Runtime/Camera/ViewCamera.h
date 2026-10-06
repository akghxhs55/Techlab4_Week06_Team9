#pragma once

struct FViewInfo;
struct FCameraMoveInput;

enum class EProjectionMode { Perspective, Orthographic };

// 카메라의 월드 위치와 회전을 담는다.
struct FCameraTransform
{
    FVector Location{ 0.0f, 0.0f, 0.0f };
    FQuat Rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
};

// 원근·직교 투영에 필요한 모드와 절두체 값을 담는다.
struct FCameraProjection
{
    EProjectionMode Mode = EProjectionMode::Perspective;
    float FovDegrees = 60.0f;
    float OrthoWidth = 16.0f;
    float NearClip = 0.1f;
    float FarClip = 10000.0f;
};

// 한 프레임의 카메라 회전·이동·줌 입력을 담는다.
struct FCameraMoveInput
{
    FVector2 MouseDelta;
    FVector MoveAxis;
    float ZoomDelta;
};

// 한 View에서 사용하는 카메라 Transform과 투영 설정을 담는다.
struct FViewCamera
{
    FCameraTransform Transform;
    FCameraProjection Projection;

    FViewInfo ToViewInfo(const FVector2& ViewSize) const;
};

// 카메라의 월드 축을 직교기저로 변환해 View 행렬을 만든다.
FMatrix BuildViewMatrix(const FCameraTransform& Transform);
// 투영 모드에 따라 원근 또는 직교 Projection 행렬을 만든다.
FMatrix BuildProjectionMatrix(const FCameraProjection& Projection, float AspectRatio);
// 로컬 이동과 Yaw·Pitch 입력을 현재 카메라에 적분한다.
FViewCamera ApplyCameraMovement(const FViewCamera& Current, const FCameraMoveInput& Input, float DeltaTime);
