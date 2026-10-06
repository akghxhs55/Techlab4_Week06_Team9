#pragma once

#include "GameFramework/Actor.h"
#include "Component/TextRenderComponent.h"

class ATextRenderActor : public AActor
{
	DECLARE_CLASS(ATextRenderActor, AActor)

	REFLECT_START(ClassName)
	REFLECT_END()

public:
	ATextRenderActor();
	virtual ~ATextRenderActor();

	virtual void DuplicateSubObjects() override;

	UTextRenderComponent* GetTextRenderComponent() const { return TextRenderComponent; }
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }

private:
	UTextRenderComponent* TextRenderComponent;
	UBillboardComponent* BillboardComponent;
};
