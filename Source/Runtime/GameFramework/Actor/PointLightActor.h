#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/PointLightComponent.h"

// 에디터에서 아이콘(빌보드)으로 보이고, 선택하면 포인트 라이트 구체가 라인으로 그려진다.
class APointLightActor : public AActor
{
	DECLARE_CLASS(APointLightActor, AActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	APointLightActor();
	virtual ~APointLightActor() override = default;

	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }

private:
	// 클릭해서 고를 수 있어야 하므로 프리미티브인 빌보드를 루트로 둔다
	UBillboardComponent* BillboardComponent = nullptr;

	UPointLightComponent* PointLightComponent = nullptr;
};
