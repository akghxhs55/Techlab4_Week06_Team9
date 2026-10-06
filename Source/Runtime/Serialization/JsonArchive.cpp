#include "EnginePCH.h"
#include "JsonArchive.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectHash.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "TypeSerializer.h"
#include "Asset/AssetManager.h"
#include "GameFramework/Actor/CameraActor.h"
#include "Component/CameraComponent.h"

namespace
{
	// "FOV": [60.0] 처럼 배열 하나로 저장된 값과 숫자 하나 모두 받는다.
	float ReadScalar(const json& Value, float Default)
	{
		if (Value.is_number())
			return Value.get<float>();
		if (Value.is_array() && !Value.empty() && Value[0].is_number())
			return Value[0].get<float>();
		return Default;
	}

	json SavePerspectiveCamera(const FViewCamera& Camera)
	{
		json Json;
		Json["Location"] = Camera.Transform.Location;
		
		const FRotator Rotation = Camera.Transform.Rotation.ToFRotator();
		Json["Rotation"] = {
			FMath::DegreesToRadians(Rotation.Roll),
			FMath::DegreesToRadians(Rotation.Pitch),
			FMath::DegreesToRadians(Rotation.Yaw)
		};
		Json["FOV"] = Camera.Projection.FovDegrees;
		Json["NearClip"] = Camera.Projection.NearClip;
		Json["FarClip"] = Camera.Projection.FarClip;
		return Json;
	}

	// 기본 씬 형식의 "PerspectiveCamera"를 메인 카메라에 적용한다.
	// Rotation은 [Roll, Pitch, Yaw] 라디안이고, 엔진 FRotator는 도 단위다 (Pitch 양수 = 아래를 봄, 씬과 같은 방향).
	FViewCamera LoadPerspectiveCamera(const json& CameraJson)
	{
		FViewCamera Result;

		if (!CameraJson.is_object())
		{
			HTR_LOG(Warning, "Invalid camera data");
			return {};
		}

		if (CameraJson.contains("Location"))
			Result.Transform.Location = CameraJson["Location"].get<FVector>();

		if (CameraJson.contains("Rotation") && CameraJson["Rotation"].is_array() && CameraJson["Rotation"].size() >= 3)
		{
			const json& R = CameraJson["Rotation"];
			Result.Transform.Rotation = FRotator(
				FMath::RadiansToDegrees(R[1].get<float>()),    // Pitch
				FMath::RadiansToDegrees(R[2].get<float>()),    // Yaw
				FMath::RadiansToDegrees(R[0].get<float>()))    // Roll
			.Quaternion();
		}

		if (CameraJson.contains("FOV"))
			Result.Projection.FovDegrees = ReadScalar(CameraJson["FOV"], Result.Projection.FovDegrees);
		if (CameraJson.contains("NearClip"))
			Result.Projection.NearClip = ReadScalar(CameraJson["NearClip"], Result.Projection.NearClip);
		if (CameraJson.contains("FarClip"))
			Result.Projection.FarClip = ReadScalar(CameraJson["FarClip"], Result.Projection.FarClip);

		return Result;
	}
}

bool FJsonArchive::SaveWorld(UWorld* World, const FViewCamera* Camera, const FString& Path)
{
	if (!World)
		return false;

	ULevel* Level = World->GetPersistentLevel();

	if (!Level)
		return false;

	json Json;

	Json["Version"] = 2;
	Json["Actors"] = json::array();

	for (AActor* Actor : Level->GetActors())
	{
		if (!Actor || Actor->HasAnyFlags(EObjectFlags::RF_Transient))
			continue;

		json ActorJson;
		ActorJson["Class"] = Actor->GetClass()->Name;
		Actor->Serialize(ActorJson["Properties"], false);

		USceneComponent* Root = Actor->GetRootComponent();
		ActorJson["RootComponentUUID"] = Root ? json(Root->GetUUID()) : json(nullptr);

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component) continue;
			json ComponentJson;
			ComponentJson["Name"] = Component->GetName();
			ComponentJson["Class"] = Component->GetClass()->Name;
			Component->Serialize(ComponentJson["Properties"], false);

			if (auto* SceneComponent = Cast<USceneComponent>(Component))
			{
				USceneComponent* Parent = SceneComponent->GetAttachParent();
				ComponentJson["ParentUUID"] = Parent ? json(Parent->GetUUID()) : json(nullptr);
			}

			ActorJson["Components"].push_back(ComponentJson);
		}

		Json["Actors"].push_back(ActorJson);
	}

	if (Camera)
		Json["PerspectiveCamera"] = SavePerspectiveCamera(*Camera);

	std::ofstream File(Path);

	if (!File.is_open())
		return false;

	File << Json.dump(4);

	File.close();

	return !File.fail();
}

bool FJsonArchive::LoadWorld(UWorld* World, FViewCamera* OutCamera, const FString& Path)
{
	if (!World)
		return false;

	if (!std::filesystem::exists(Path))
	{
		HTR_LOG(Warning, "{} is Not Exist!", Path);
		return false;
	}

	std::ifstream File(Path);

	if (!File.is_open())
		return false;

	json Json;

	try
	{
		File >> Json;
	}
	catch (const json::parse_error&)
	{
		return false;
	}

	// 임시 DefaultScene.Scene 로딩용
	if (Json.contains("Primitives"))
	{
		World->ClearWorld();
		for (const auto& [KeyString, PrimJson] : Json["Primitives"].items())
		{
			FTransform Transform;
			Transform.Location = PrimJson["Location"].get<FVector>();
			Transform.Rotation = PrimJson["Rotation"].get<FRotator>();
			Transform.Scale = PrimJson["Scale"].get<FVector>();

			AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(NAME_None, &Transform);
			UStaticMesh* Mesh = UAssetManager::GetAssetByPath<UStaticMesh>(PrimJson["ObjStaticMeshAsset"].get<FString>());
			Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		}
		World->GetScene().BuildBVH();

		if (OutCamera && Json.contains("PerspectiveCamera"))
			*OutCamera = LoadPerspectiveCamera(Json["PerspectiveCamera"]);

		return true;
	}

	if (!Json.contains("Version") || Json["Version"] != 2)
		return false;

	// 1. 모든 액터의 기본 정보를 먼저 검사
	for (const json& ActorJson : Json["Actors"])
	{
		if (!ActorJson.is_object())
			return false;

		if (!ActorJson.contains("Class") ||
			!ActorJson["Class"].is_string())
			return false;

		UClass* Class =
			FindClass(ActorJson["Class"].get<FString>());

		if (!Class || !Class->IsChildOf(AActor::StaticClass()))
			return false;
	}

	if (!Json.contains("Actors") || !Json["Actors"].is_array())
		return false;

	World->ClearWorld();

	// 3. 실제 생성
	for (json& ActorJson : Json["Actors"])
	{
		if (!ActorJson.is_object())
			return false;
		if (!ActorJson.contains("Class") || !ActorJson["Class"].is_string())
			return false;

		UClass* Class = FindClass(ActorJson["Class"].get<FString>());
		if (!Class || !Class->IsChildOf(AActor::StaticClass()))
			return false;

		AActor* Actor = World->SpawnActor(Class);
		if (!Actor)
		{
			HTR_LOG(Warning, "Load: failed to spawn {}", ActorJson["Class"].get<FString>());
			continue;
		}
		Actor->Serialize(ActorJson["Properties"], true);

		TMap<uint32, USceneComponent*> ComponentMap; // UUID → Component

		struct FPendingAttachment
		{
			USceneComponent* Child = nullptr;
			bool bHasParent = false;
			uint32 ParentUUID = 0;
		};
		TArray<FPendingAttachment> PendingAttachments;

		for (json& ComponentJson : ActorJson["Components"])       // json → json&
		{
			const FName Name(ComponentJson["Name"].get<FString>());

			// 이름으로 컴포넌트 찾기
			UActorComponent* Component = nullptr;
			for (UActorComponent* C : Actor->GetComponents())
			{
				if (C && C->GetFName() == Name)
				{
					Component = C;
					break;
				}
			}

			if (!Component)
			{
				//HTR_LOG(Warning, "Load: {} has no component {}", Class->Name, Name.ToString());
				//continue;
				UClass* ComponentClass =
					FindClass(ComponentJson["Class"].get<FString>());

				if (!ComponentClass ||
					!ComponentClass->IsChildOf(UActorComponent::StaticClass()) ||
					!ComponentClass->Constructor)
				{
					HTR_LOG(Warning, "Load: invalid component class {}",
						ComponentJson["Class"].get<FString>());
					continue;
				}

				// 부모 연결은 아래 계층 복원 단계에서 처리한다.
				Component = Actor->AddComponentByClass(ComponentClass, true);

				if (!Component)
					continue;

				Component->SetName(Name);
			}

			if (Component->GetClass()->Name != ComponentJson["Class"].get<FString>())
			{
				HTR_LOG(Warning, "Load: component {} class mismatch", Name.ToString());
				continue;
			}

			Component->Serialize(ComponentJson["Properties"], true);

			// Enqueue attachment for later processing, since the parent may not have been created yet
			if (auto* SceneComponent = Cast<USceneComponent>(Component))
			{
				ComponentMap.Add(SceneComponent->GetUUID(), SceneComponent);

				if (ComponentJson.contains("ParentUUID"))
				{
					const json& ParentUUID = ComponentJson["ParentUUID"];
					const bool bHasParent = !ParentUUID.is_null();

					PendingAttachments.Add({
						SceneComponent,
						bHasParent,
						bHasParent ? ParentUUID.get<uint32>() : 0
						});
				}
			}
		}

		for (const FPendingAttachment& Pending : PendingAttachments)
		{
			Pending.Child->DetachFromParent(); // Detach first to avoid issues
		}

		// Now attach the components to their parents
		for (const FPendingAttachment& Pending : PendingAttachments)
		{
			if (!Pending.bHasParent)
				continue;

			USceneComponent** Parent = ComponentMap.Find(Pending.ParentUUID);

			if (Parent && *Parent)
			{
				Pending.Child->SetupAttachment(*Parent);
			}
			else
			{
				HTR_LOG(Warning, "Load: component {} parent UUID {} not found",
					Pending.Child->GetName(), Pending.ParentUUID);
			}

		}

		if (ActorJson.contains("RootComponentUUID"))
		{
			const json& RootUUID = ActorJson["RootComponentUUID"];

			if (RootUUID.is_null())
			{
				Actor->SetRootComponent(nullptr);
			}
			else
			{
				USceneComponent** Root = ComponentMap.FindOrNull(RootUUID.get<uint32>());

				if (Root && *Root)
					Actor->SetRootComponent(*Root);
			}
		}

		for (const auto& Entry : ComponentMap)
		{
			Entry.second->MarkTransformDirty();
		}
	}

	//if (!Json.contains("NextUUID"))
	//	return false;

	//// 파일 검증이 끝난 뒤 Clear
	//uint64 SavedNextUUID = Json["NextUUID"].get<uint64>();

	//World->ClearScene();

	//FEngineStatics::NextUUID = SavedNextUUID;

	//if (!Json.contains("Primitives"))
	//{
	//	return true;
	//}

	//for (auto& [UUIDString, PrimitiveJson] : Json["Primitives"].items())
	//{
	//	uint64 UUID = std::stoull(UUIDString);

	//	FTransform Transform;

	//	if (PrimitiveJson.contains("Location"))
	//	{
	//		Transform.Location.X = PrimitiveJson["Location"][0].get<float>();
	//		Transform.Location.Y = PrimitiveJson["Location"][1].get<float>();
	//		Transform.Location.Z = PrimitiveJson["Location"][2].get<float>();
	//	}

	//	if (PrimitiveJson.contains("Rotation"))
	//	{
	//		Transform.Rotation.Roll = PrimitiveJson["Rotation"][0].get<float>();
	//		Transform.Rotation.Pitch = PrimitiveJson["Rotation"][1].get<float>();
	//		Transform.Rotation.Yaw = PrimitiveJson["Rotation"][2].get<float>();
	//	}

	//	if (PrimitiveJson.contains("Scale"))
	//	{
	//		Transform.Scale.X = PrimitiveJson["Scale"][0].get<float>();
	//		Transform.Scale.Y = PrimitiveJson["Scale"][1].get<float>();
	//		Transform.Scale.Z = PrimitiveJson["Scale"][2].get<float>();
	//	}

	//	if (!PrimitiveJson.contains("Type"))
	//		continue;

	//	FString TypeString = PrimitiveJson["Type"].get<FString>();

	//	if (TypeString == "Other")
	//		continue;

	//	EPrimitiveType Type = FStringToPrimitiveType(TypeString);

	//	AStaticMeshActor* Actor =
	//		World->SpawnActor<AStaticMeshActor>("Test", &Transform);

	//	if (!Actor)
	//		continue;

	//	Actor->SetPrimitiveType(Type);
	//	Actor->SetUUID(UUID);
	//}

	//// SpawnActor하면서 증가했을 UUID를 저장 당시 값으로 복원
	//FEngineStatics::NextUUID = SavedNextUUID;

	if (OutCamera && Json.contains("PerspectiveCamera"))
		*OutCamera = LoadPerspectiveCamera(Json["PerspectiveCamera"]);
	World->GetScene().BuildBVH();

	return true;
}