#include "EnginePCH.h"
#include "Component/PointLightComponent.h"

#include "Render/LineBatcher.h"

namespace
{
	constexpr int32 CIRCLE_SEGMENT_COUNT = 32;

	// Center를 중심으로 AxisA와 AxisB가 이루는 평면에 반지름 Radius의 와이어 원을 그린다.
	void AddWireCircle(
		FLineBatcher* LineBatcher,
		const FVector& Center,
		const FVector& AxisA,
		const FVector& AxisB,
		float Radius,
		const FVector4& Color)
	{
		FVector PreviousPoint;
		FVector FirstPoint;

		for (int32 i = 0; i < CIRCLE_SEGMENT_COUNT; ++i)
		{
			const float Angle = (2.0f * PI * i) / CIRCLE_SEGMENT_COUNT;
			const FVector Point = Center + AxisA * (std::cosf(Angle) * Radius) + AxisB * (std::sinf(Angle) * Radius);

			if (i == 0)
			{
				FirstPoint = Point;
			}
			else
			{
				LineBatcher->AddLine(PreviousPoint, Point, Color);
			}

			PreviousPoint = Point;
		}

		LineBatcher->AddLine(PreviousPoint, FirstPoint, Color);
	}
}

void UPointLightComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	if (LineBatcher == nullptr)
	{
		return;
	}

	const FVector Center = GetWorldLocation();
	const float SafeRadius = (AttenuationRadius > 0.0f) ? AttenuationRadius : 0.0f;
	if (SafeRadius <= 0.0001f)
	{
		return;
	}

	// 3개의 직교 평면(XY, XZ, YZ)에 원을 그려 와이어프레임 구체 형성
	AddWireCircle(LineBatcher, Center, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), SafeRadius, LightColor);
	AddWireCircle(LineBatcher, Center, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), SafeRadius, LightColor);
	AddWireCircle(LineBatcher, Center, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), SafeRadius, LightColor);
}
