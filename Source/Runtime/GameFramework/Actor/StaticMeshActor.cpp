#include "EnginePCH.h"
#include "StaticMeshActor.h"

#include "Component/PrimitiveComponent.h"
#include "ObjectSystem/ObjectFactory.h"
#include "Asset/AssetManager.h"

AStaticMeshActor::AStaticMeshActor()
{
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("UStaticMeshComponent");
	SetRootComponent(StaticMeshComponent);

}

void AStaticMeshActor::SetPrimitiveType(EPrimitiveType Type)
{

}


void AStaticMeshActor::BeginPlay()
{
	Super::BeginPlay();
}

void AStaticMeshActor::DuplicateSubObjects()
{
	int32 Index = -1;
	for (int32 i = 0; i < GetComponents().Num(); ++i)
	{
		if (GetComponents()[i] == StaticMeshComponent)
		{
			Index = i;
		}
	}

	Super::DuplicateSubObjects();

	StaticMeshComponent = Index != -1 ? Cast<UStaticMeshComponent>(GetComponents()[Index]) : nullptr;
}
