#include "EnginePCH.h"

#include "GameFramework/Actor/CameraActor.h"

#include "Asset/AssetManager.h"
#include "Component/BillboardComponent.h"
#include "Component/CameraComponent.h"

// 카메라 컴포넌트를 기본 서브오브젝트로 만들어 루트에 연결한다.
ACameraActor::ACameraActor()
{
	CameraComponent = CreateDefaultSubobject<UCameraComponent>("CameraComponent");
	SetRootComponent(CameraComponent);

	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("UBillboardComponent");
	BillboardComponent->SetupAttachment(CameraComponent);
	BillboardComponent->SetMaterial(0, UAssetManager::GetAssetByPath<UMaterial>("CameraIcon"));
	BillboardComponent->SetEditorOnly(true);
	BillboardComponent->SetHideInDetails(true);
}

void ACameraActor::DuplicateSubObjects()
{
	int32 CameraIndex = -1;
	int32 BillboardIndex = -1;
	for (int32 i = 0; i < Components.Num(); ++i)
	{
		if (Components[i] == CameraComponent)
		{
			CameraIndex = i;
		}
		else if (Components[i] == BillboardComponent)
		{
			BillboardIndex = i;
		}
	}

	Super::DuplicateSubObjects();

	CameraComponent = CameraIndex != -1 ? Cast<UCameraComponent>(Components[CameraIndex]) : nullptr;
	BillboardComponent = BillboardIndex != -1 ? Cast<UBillboardComponent>(Components[BillboardIndex]) : nullptr;
}
