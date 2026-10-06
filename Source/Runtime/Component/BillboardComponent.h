#pragma once

#include "PrimitiveComponent.h"

struct FRenderView;

class UBillboardComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UBillboardComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		PROPERTY(Material)
	REFLECT_END()

public:
	UBillboardComponent();
	virtual ~UBillboardComponent() override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual int32 GetNumMaterials() const override { return 1; }
	virtual UMaterial* GetMaterial(int32 SlotIndex) const override { return SlotIndex == 0 ? Material : nullptr; }
	virtual void SetMaterial(int32 SlotIndex, UMaterial* InMaterial) override { if (SlotIndex == 0) Material = InMaterial; }

	virtual const FStaticMeshData* GetMeshData() const override { return QuadMesh ? &QuadMesh->GetMeshData() : nullptr; }

	// View별 Adapter가 계산한 Billboard 행렬을 사용해 같은 렌더 패킷 형식으로 제출한다.
	void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FMatrix& BillboardWorldMatrix);
	FMatrix GetBillboardMatrix(const FRenderView& RenderView) const;

	virtual void Serialize(json& Handle, bool bIsLoading) override;

protected:
	UMaterial* Material = nullptr;
	UStaticMesh* QuadMesh = nullptr;
private:

};
