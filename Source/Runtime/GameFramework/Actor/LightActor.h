#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/SpotLightComponent.h"

// 에디터에서 아이콘(빌보드)으로 보이고, 선택하면 스포트라이트 원뿔이 라인으로 그려진다.
class ALightActor : public AActor
{
	DECLARE_CLASS(ALightActor, AActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	ALightActor();
	virtual ~ALightActor() override = default;

	USpotLightComponent* GetSpotLightComponent() const { return SpotLightComponent; }
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }

	virtual void DuplicateSubObjects() override;

private:
	USpotLightComponent* SpotLightComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
};
