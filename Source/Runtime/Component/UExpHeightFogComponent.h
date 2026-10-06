#pragma once 

#include "SceneComponent.h"

struct FogData {
	// 안개의 전체 밀도 
	float FogDensity = 0.02f;
	// 높이에 따라 밀도가 감소하는 정도 
	float FogHeightFalloff = 0.2f;
	// 안개가 시작되는 높이 (월드 좌표 Z)
	float FogHeightStart = 0.0f;
	// 안개의 기본 산란 색 
	FVector4 FogInscatteringLuminance = FVector4(0.5f, 0.5f, 0.5f, 1.0f);
	// 안개의 최대 불투명도 
	float FogMaxOpacity = 1.0f;
	// 카메라로부터 안개 계산 시작 거리 
	float StartDistance = 0.0f;
	// 안개 적분을 끝낼 거리 
	float EndDistance = 1000.0f;
	// 이 거리보다 먼 물체에는 안개를 적용하지 않는다. 
	float FogCutOffDistance = 1000.0f;
};

class UExpHeightFogComponent : public USceneComponent {
	DECLARE_CLASS(UExpHeightFogComponent, USceneComponent)
	
	REFLECT_START(ClassName)
		PROPERTY(FogParam.FogDensity)
		PROPERTY(FogParam.FogHeightFalloff)
		PROPERTY_TYPE(FogParam.FogInscatteringLuminance, Color)
		PROPERTY(FogParam.FogMaxOpacity)
		PROPERTY(FogParam.StartDistance)
		PROPERTY(FogParam.EndDistance)
		PROPERTY(FogParam.FogCutOffDistance)
	REFLECT_END()
public:
	UExpHeightFogComponent();
	virtual ~UExpHeightFogComponent();
	
	FogData& GetFogData();

private:
	FogData FogParam;
};