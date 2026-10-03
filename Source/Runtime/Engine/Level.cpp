#include "EnginePCH.h"
#include "Level.h"

// delete Actor가 소멸자를 호출하려면 AActor의 완전한 정의가 필요하다.
// 전방 선언만으로는 C4150(소멸자 미호출)이 되어 액터가 GUObjectArray에 남는다.
#include "GameFramework/Actor.h"

ULevel::ULevel(const ULevel& Other)
	: UObject(Other)
	, Actors(Other.Actors)
	, bIsVisible(Other.bIsVisible)
{
}

void ULevel::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	for (AActor*& Actor : Actors)
	{
		if (Actor)
		{
			Actor = Actor->Duplicate<AActor>();
		}
	}

	// OwningWorld is set by the caller (UWorld::DuplicateSubObjects)
}

void ULevel::AddActor(AActor* Actor)
{
	if (!Actor)
		return;

	Actors.Add(Actor);
}

void ULevel::ClearActors()
{
	for (AActor* Actor : Actors)
	{
		if (!Actor)
			continue;

		delete Actor;
	}

	Actors.Reset();
}