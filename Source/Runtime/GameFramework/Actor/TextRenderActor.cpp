#include "EnginePCH.h"
#include "TextRenderActor.h"

#include "Asset/AssetManager.h"

ATextRenderActor::ATextRenderActor()
{
	TextRenderComponent = CreateDefaultSubobject<UTextRenderComponent>("TextRenderComponent");
	SetRootComponent(TextRenderComponent);

	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>("BillboardComponent");
	BillboardComponent->SetupAttachment(TextRenderComponent);
	BillboardComponent->SetMaterial(0, UAssetManager::GetAssetByPath<UMaterial>("TextRenderIcon"));
	BillboardComponent->SetEditorOnly(true);
	BillboardComponent->SetHideInDetails(true);
}

ATextRenderActor::~ATextRenderActor()
{
}

void ATextRenderActor::DuplicateSubObjects()
{
	int32 TextRenderIndex = -1;
	int32 BillboardIndex = -1;
	for (int32 i = 0; i < GetComponents().Num(); ++i)
	{
		if (GetComponents()[i] == TextRenderComponent)
		{
			TextRenderIndex = i;
		}
		else if (GetComponents()[i] == BillboardComponent)
		{
			BillboardIndex = i;
		}
	}

	Super::DuplicateSubObjects();

	TextRenderComponent = TextRenderIndex != -1 ? Cast<UTextRenderComponent>(GetComponents()[TextRenderIndex]) : nullptr;
	BillboardComponent = BillboardIndex != -1 ? Cast<UBillboardComponent>(GetComponents()[BillboardIndex]) : nullptr;
}
