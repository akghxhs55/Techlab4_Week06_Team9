#pragma once

#include "GameFramework/Actor.h"
#include "ObjectSystem/Class.h"

class UBillboardComponent;
class UCameraComponent;

class ACameraActor : public AActor
{
	DECLARE_CLASS(ACameraActor, AActor)

	REFLECT_START(ClassName)
		PROPERTY(bIsMainCamera)
	REFLECT_END()

public:
	// 카메라 컴포넌트를 기본 서브오브젝트로 생성해 Actor 루트에 연결한다.
	ACameraActor();

	UCameraComponent* GetCameraComponent() const { return CameraComponent; }
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }

	virtual void DuplicateSubObjects() override;

	bool bIsMainCamera = true;

private:
	UCameraComponent* CameraComponent;
	UBillboardComponent* BillboardComponent;
};
