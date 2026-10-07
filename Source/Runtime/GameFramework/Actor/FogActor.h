#pragma once

#include "GameFramework/Actor.h"
#include "Component/BillboardComponent.h"
#include "Component/UExpHeightFogComponent.h"

class AFogActor : public AActor {
	DECLARE_CLASS(AFogActor, AActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	AFogActor();
	virtual ~AFogActor() override = default;

	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	UExpHeightFogComponent* GetExpHeightFogComponent() const { return ExpHeightFogComponent; }

	virtual void DuplicateSubObjects() override;

private:
	// 클릭해서 고를 수 있어야 하므로 프리미티브인 빌보드를 루트로 둔다
	UBillboardComponent* BillboardComponent = nullptr;
	UExpHeightFogComponent* ExpHeightFogComponent = nullptr;
};
