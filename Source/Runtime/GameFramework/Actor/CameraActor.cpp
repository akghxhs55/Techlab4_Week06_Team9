#include "EnginePCH.h"

#include "GameFramework/Actor/CameraActor.h"

#include "Asset/AssetManager.h"
#include "Component/BillboardComponent.h"
#include "Component/CameraComponent.h"

// 카메라 컴포넌트를 기본 서브오브젝트로 만들어 루트에 연결한다.
ACameraActor::ACameraActor()
{
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	SetRootComponent(BillboardComponent);
	BillboardComponent->SetMaterial(0, UAssetManager::GetAssetByPath<UMaterial>("CameraIcon"));

	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");
	CameraComponent->SetupAttachment(BillboardComponent);
}

void ACameraActor::DuplicateSubObjects()
{
	int32 BillboardIndex = -1;
	int32 CameraIndex = -1;
	for (int32 i = 0; i < Components.Num(); ++i)
	{
		if (Components[i] == BillboardComponent)
		{
			BillboardIndex = i;
		}
		else if (Components[i] == CameraComponent)
		{
			CameraIndex = i;
		}
	}

	Super::DuplicateSubObjects();

	BillboardComponent = BillboardIndex != -1 ? Cast<UBillboardComponent>(Components[BillboardIndex]) : nullptr;
	CameraComponent = CameraIndex != -1 ? Cast<UCameraComponent>(Components[CameraIndex]) : nullptr;
}
