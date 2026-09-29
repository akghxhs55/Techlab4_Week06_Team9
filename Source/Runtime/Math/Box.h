#pragma once

struct FBox
{
	FVector Min;
	FVector Max;

	FBox GetWorldAABB(const FMatrix& M) const
	{
		FVector Center = (Min + Max) * 0.5f;
		FVector Extent = (Max - Min) * 0.5f;

		// 중심은 그냥 변환
		FVector4 C = FVector4(Center, 1.0f) * M;

		// 범위는 회전 부분의 절댓값으로 변환
		FVector E;
		E.X = Extent.X * fabsf(M.M[0][0]) + Extent.Y * fabsf(M.M[1][0]) + Extent.Z * fabsf(M.M[2][0]);
		E.Y = Extent.X * fabsf(M.M[0][1]) + Extent.Y * fabsf(M.M[1][1]) + Extent.Z * fabsf(M.M[2][1]);
		E.Z = Extent.X * fabsf(M.M[0][2]) + Extent.Y * fabsf(M.M[1][2]) + Extent.Z * fabsf(M.M[2][2]);

		return FBox{ FVector(C.X, C.Y, C.Z) - E, FVector(C.X, C.Y, C.Z) + E };
	}

	void Expand(const FBox& Other)
	{
		Min.X = std::min(Min.X, Other.Min.X);
		Min.Y = std::min(Min.Y, Other.Min.Y);
		Min.Z = std::min(Min.Z, Other.Min.Z);

		Max.X = std::max(Max.X, Other.Max.X);
		Max.Y = std::max(Max.Y, Other.Max.Y);
		Max.Z = std::max(Max.Z, Other.Max.Z);
	}
};
