#include "EnginePCH.h"
#include "World.h"
#include "Level.h"

#include "ObjectSystem/ObjectFactory.h"
#include "Core/EngineStatics.h"
#include "GameFramework/Actor/StaticMeshActor.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Input/InputSystem.h"

#include "UObject/UObjectIterator.h"

#include "Collision/Ray.h"
#include "Component/BillboardComponent.h"

#include "Component/StaticMeshComponent.h"
#include "Asset/LOD/StaticMeshLODSelector.h"

#include "Math/Frustum.h"

#include "Core/Stats/LightweightStats.h"

DECLARE_CYCLE_STAT("Actor Tick", STAT_ActorTick); // Actor 틱 측정
DECLARE_CYCLE_STAT("Update All Transforms", STAT_UpdateAllTransforms); // 각 Transform의 Update 시간 측정
DECLARE_CYCLE_STAT("Gather Render Packets", STAT_GatherRenderPackets);
DECLARE_CYCLE_STAT("Frustum Cull", STAT_FrustumCull);
DECLARE_CYCLE_STAT("Gather Elements", STAT_GatherElements);



UWorld::~UWorld()
{

}

bool  UWorld::Init()
{
	// Spawn Actor로 카메라 생성하고 세팅하기
	PersistentLevel = FObjectFactory::ConstructObject<ULevel>();

	if (!PersistentLevel)
	{
		HTR_LOG(Error, "Failed to create PersistentLevel");
		return false;
	}

	//레벨 연결
	PersistentLevel->SetWorld(this);
	Levels.Add(PersistentLevel);
	CurrentLevel = PersistentLevel;

	//카메라 생성
	CreateMainCamera();

	return true;
}

AActor* UWorld::SpawnActor(UClass* Class, FName InName, const FTransform* Transform)
{
	if (!Class) return nullptr;
	if (!Class->IsChildOf(AActor::StaticClass())) return nullptr;

	// 1. ObjectFactory로 Actor 생성
	AActor* NewActor = Cast<AActor>(FObjectFactory::ConstructObject(Class, PersistentLevel, InName));

	if (!NewActor)
	{
		HTR_LOG(Error, "SpawnActor : Failed to create Actor");
		return nullptr;
	}

	// 2. Actor에 World/Level 연결
	NewActor->World = this;
	NewActor->Level = PersistentLevel;

	// 3. Transform 적용
	const FTransform SpawnTransform = Transform ? *Transform : FTransform::Identity;

	if (NewActor->GetRootComponent())
	{
		NewActor->GetRootComponent()->SetTransform(SpawnTransform);
	}

	for (UActorComponent* Component : NewActor->GetComponents())
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			Scene.AddPrimitive(Primitive);
	}

	// 4. Level->Actors에 등록
	PersistentLevel->AddActor(NewActor);

	// 5. PlayList에 추가
	BeginPlayList.Enqueue(NewActor);

	return NewActor;
}

void UWorld::Tick(float DeltaTime)
{
	while (!BeginPlayList.IsEmpty())
	{
		BeginPlayList.Peek()->BeginPlay();
		BeginPlayList.Dequeue();
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ActorTick);
		for (ULevel* Level : Levels)
		{
			for (AActor* Actor : Level->GetActors())
			{
				Actor->Tick(DeltaTime);
			}
			PathTracker.Tick(Level->GetActors(), DeltaTime);
		}

		if (MainCamera)
		{
			MainCamera->Tick(DeltaTime);
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_UpdateAllTransforms);
		Scene.UpdateAllTransforms();
	}
}

void UWorld::ClearWorld()
{
	// BeginPlay 대기 중인 Actor 제거
	while (!BeginPlayList.IsEmpty())
	{
		BeginPlayList.Dequeue();
	}

	PathTracker.SetPlaybackEnabled(false);
	PathTracker.SetPathRenderingEnabled(false);
	PathTracker.ClearPath();

	for (ULevel* Level : Levels)
	{
		Level->ClearActors();
	}
	HTR_LOG(Info, "{} : ", PersistentLevel->GetActorNum());
}

void UWorld::GatherRenderPackets(TQueue<FRenderPacket>& RenderQueue, const FLODViewContext* LODView, const FFrustumPlanes* Frustum)
{
	TArray<FPrimitiveSceneProxy*> VisibleProxies;
	{
		SCOPE_CYCLE_COUNTER(STAT_FrustumCull);          
		const int32 Count = Scene.Proxies.Num();
		Scene.BVH.Query(
			[&](const FBox& Bounds) {
				return !Frustum || IsAABBInFrustum(MakeWorldBounds(Bounds), *Frustum);
			},
			[&](UPrimitiveComponent* Component) {
				if (Component && Component->IsVisible())
					VisibleProxies.Add(Component->GetSceneProxy());
			});
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_GatherElements);
		for (FPrimitiveSceneProxy* Proxy : VisibleProxies)
		{
			const FMatrix& World = Proxy->GetLocalToWorld();

			if (LODView)
			{
				if (auto* Component = Cast<UStaticMeshComponent>(Proxy->GetComponent()))
				{
					if (UStaticMesh* Mesh =
						Component->GetStaticMesh())
					{
						const uint32 LOD = SelectStaticMeshLOD(
							*Mesh,
							World,
							*LODView);

						Component->SubmitToRenderQueue(RenderQueue, LOD);
						continue;
					}
				}
			}

			Proxy->GetComponent()->SubmitToRenderQueue(RenderQueue);
		}
	}
}

//void UWorld::GatherRenderPackets(TArray<FRenderPacket>& RenderArray, const FLODViewContext* LODView, const FFrustumPlanes* Frustum)
//{
//	for (TObjectIterator<UPrimitiveComponent> Itr; Itr; ++Itr)
//	{
//		if (!*Itr || !Itr->IsVisible())
//			continue;
//
//		if (Frustum)
//		{
//			const FBox Box = Itr->CalcBounds();
//			const FAABB Bounds{
//				(Box.Min + Box.Max) * 0.5f,
//				(Box.Max - Box.Min) * 0.5f
//			};
//			if (!IsAABBInFrustum(Bounds, *Frustum))
//				continue;
//		}
//
//		if (LODView)
//		{
//			if (auto* Component = Cast<UStaticMeshComponent>(*Itr))
//			{
//				if (UStaticMesh* Mesh =
//					Component->GetStaticMesh())
//				{
//					const uint32 LOD = SelectStaticMeshLOD(
//						*Mesh,
//						Component->GetWorldMatrix(),
//						*LODView);
//
//					Component->SubmitToRenderQueue(RenderQueue, LOD);
//					continue;
//				}
//			}
//		}
//
//		Itr->SubmitToRenderQueue(RenderQueue);
//	}
//}

// 메인 카메라 생성
void UWorld::CreateMainCamera()
{
	if (MainCamera)
		return;

	MainCamera = FObjectFactory::ConstructObject<ACameraActor>();

	if (!MainCamera)
	{
		HTR_LOG(Error, "Failed to create MainCamera");
		return;
	}

	MainCamera->World = this;
	MainCamera->Level = nullptr;
	MainCamera->GetCameraComponent()->SetRelativeLocation(FVector(-5.0f, -5.0f, 5.0f));
}

int32 UWorld::GetActorNum()
{
	return PersistentLevel->GetActorNum();
}

bool UWorld::DestroyActor(AActor* Actor)
{
	if (!Actor)
		return false;

	ULevel* Level = Actor->GetLevel();

	if (!Level)
		return false;

	// 1. BeginPlay 대기열에서 제거
	TQueue<AActor*> NewBeginPlayList;

	while (!BeginPlayList.IsEmpty())
	{
		AActor* PendingActor = BeginPlayList.Peek();
		BeginPlayList.Dequeue();

		if (PendingActor != Actor)
		{
			NewBeginPlayList.Enqueue(PendingActor);
		}
	}

	BeginPlayList = std::move(NewBeginPlayList);

	// 2. PathTracker에서 제거
	PathTracker.OnObjectDestroyed(Actor);

	//// 3. PrimitiveComponents에서 제거
	//for (UActorComponent* Component : Actor->Components)
	//{
	//	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);

	//	if (!Primitive)
	//		continue;

	//	for (int32 i = PrimitiveComponents.Num() - 1; i >= 0; --i)
	//	{
	//		if (PrimitiveComponents[i] == Primitive)
	//		{
	//			PrimitiveComponents.RemoveAt(i, 1);
	//			break;
	//		}
	//	}
	//}

	// 4. Level의 Actors에서 제거
	for (int32 i = Level->Actors.Num() - 1; i >= 0; --i)
	{
		if (Level->Actors[i] == Actor)
		{
			Level->Actors.RemoveAt(i, 1);
			break;
		}
	}

	FString ActorName = Actor->GetName();
	uint32 ActorUUID = Actor->GetUUID();

	// 6. Actor 삭제
	delete Actor;

	HTR_LOG(Info, "Destroy Actor : {} UUID {}", ActorName, ActorUUID);

	return true;
}

// 다른 World의 객체를 제외하고 Component 교차 중 최근접 결과를 선택한다.
bool UWorld::LineTraceSingle(const FRay& WorldRay, FHitResult& OutHit,
	FBillboardTraceTransform ResolveBillboard, const void* ViewContext)
{
	OutHit = FHitResult();
	float NearestT = std::numeric_limits<float>::max();

	const auto TraceComponent = [&](UPrimitiveComponent* Component, float& OutNearestT)
	{
		if (!Component || !Component->IsVisible())
			return false;

		FHitResult Hit;
		bool bHit = false;

		if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component);
			Billboard && ResolveBillboard)
		{
			bHit = Billboard->LineTraceComponentForView(WorldRay, Hit, ResolveBillboard(*Billboard, ViewContext));
		}
		else
		{
			bHit = Component->LineTraceComponent(WorldRay, Hit);
		}

		if (!bHit ||
			!Hit.HitComponent ||
			Hit.Distance < 0.0f ||
			Hit.Distance >= OutNearestT)
		{
			return false;
		}

		OutHit = Hit;
		OutNearestT = Hit.Distance;
		return true;
	};

	Scene.BVH.TraceClosest(
		[&](const FBox& Bounds, float& OutEnterT) { return RayIntersectsAABB(WorldRay, Bounds.Min, Bounds.Max, OutEnterT); }, 
		TraceComponent,
		NearestT);

	return OutHit.HitComponent != nullptr;
}

void UWorld::BeginPlay()
{
}

void UWorld::EndPlay()
{
}
