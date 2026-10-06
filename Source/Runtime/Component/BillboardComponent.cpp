#include "EnginePCH.h"
#include "BillboardComponent.h"

#include "ParticleSubUVComponent.h"
#include "Asset/AssetManager.h"
#include "Camera/RenderView.h"
#include "GameFramework/Actor.h"
#include "Component/CameraComponent.h"
#include "Engine/World.h"

// Billboard 컴포넌트의 초기 상태를 구성한다.
UBillboardComponent::UBillboardComponent()
{
	QuadMesh = UAssetManager::GetAssetByPath<UStaticMesh>("ParticleQuad");
	Material = UAssetManager::GetAssetByPath<UMaterial>("BillboardIcon");
}

// Billboard 컴포넌트의 소멸을 처리한다.
UBillboardComponent::~UBillboardComponent()
{
}

// 기본 Mesh와 Material을 에셋 관리자에서 가져온다.
void UBillboardComponent::BeginPlay()
{
	Super::BeginPlay();

}

// 부모 컴포넌트의 프레임 갱신을 호출한다.
void UBillboardComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

FBox UBillboardComponent::CalcLocalBounds() const
{
	const FVector Scale = GetWorldScale3D();
	const float Radius = 0.5f * std::sqrt(Scale.Y * Scale.Y + Scale.Z * Scale.Z);
	return FBox(FVector(-Radius), FVector(Radius));
}

// View별 Billboard 행렬과 Material을 렌더 패킷에 담는다.
void UBillboardComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FMatrix& BillboardWorldMatrix)
{
	if (QuadMesh == nullptr || Material == nullptr)
		return;

	static const FSubUVConstants MaterialParam{
		.AtlasRowSize = 1,
		.AtlasColSize = 1,
		.Alpha = 1.0f
	};

	FRenderPacket Packet;
	Packet.Mesh = QuadMesh;
	Packet.Material = Material;
	Packet.Model = RenderQueue.StoreWorldMatrix(BillboardWorldMatrix);
	Packet.MaterialParamData = &MaterialParam;
	Packet.MaterialParamDataSize = sizeof(FSubUVConstants);
	RenderQueue.Add(Packet);
}

FMatrix UBillboardComponent::GetBillboardMatrix(const FRenderView& RenderView) const
{
	const FVector Scale = GetWorldScale3D();
	return RenderView.BuildBillboardMatrix(GetWorldLocation(), Scale.Y, Scale.Z);
}

void UBillboardComponent::Serialize(json& Handle, bool bIsLoading)
{
	Super::Serialize(Handle, bIsLoading);

	if (bIsLoading)
	{
		// 예전 파일은 "Material"이 문자열(경로)이라 형식을 확인하고 읽는다
		if (!Handle.contains("Material"))
		{
			return;
		}

		const json& MaterialJson = Handle["Material"];

		if (MaterialJson.is_object())
		{
			UMaterial* LoadedMaterial = UMaterial::LoadMaterial(MaterialJson);

			if (LoadedMaterial)
			{
				SetMaterial(0, LoadedMaterial);
			}
			else
			{
				HTR_LOG(Warning, "Load: failed to restore material for {}", GetName());
			}
		}
		else
		{
			HTR_LOG(Warning, "Load: invalid material data for {}, expected object but got {}", GetName(), MaterialJson.type_name());
		}
	}
	else
	{
		Handle["Material"] = Material ? UMaterial::SaveMaterial(Material) : json(nullptr);
	}
}
