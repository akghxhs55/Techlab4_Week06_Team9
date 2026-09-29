#include "EnginePCH.h"
#include "StaticMeshData.h"

#include <format>

bool FStaticMeshData::Validate(FString& OutError) const
{
	const uint32 VertexCount = static_cast<uint32>(Vertices.Num());
	const uint32 IndexCount = static_cast<uint32>(Indices.Num());

	if (VertexCount == 0 || IndexCount == 0 || IndexCount % 3 != 0)
	{
		OutError = std::format("invalid mesh (vertices {}, indices {})", VertexCount, IndexCount);
		return false;
	}

	for (uint32 Index : Indices)
	{
		if (Index >= VertexCount)
		{
			OutError = std::format("index {} out of range (vertices {})", Index, VertexCount);
			return false;
		}
	}

	uint32 SectionIndexSum = 0;
	for (const FStaticMeshSection& Section : Sections)
	{
		if (Section.StartIndex + Section.IndexCount > IndexCount)
		{
			OutError = std::format("section range {}+{} exceeds indices {}", Section.StartIndex, Section.IndexCount, IndexCount);
			return false;
		}
		if (Section.MaterialSlotIndex >= static_cast<uint32>(MaterialSlots.Num()))
		{
			OutError = std::format("material slot {} out of range (slots {})", Section.MaterialSlotIndex, MaterialSlots.Num());
			return false;
		}
		SectionIndexSum += Section.IndexCount;
	}
	if (SectionIndexSum != IndexCount)
	{
		OutError = std::format("section index sum {} != indices {}", SectionIndexSum, IndexCount);
		return false;
	}

	if (!LODSourceVertices.IsEmpty() &&
		LODSourceVertices.Num() != Vertices.Num())
	{
		OutError = "LOD source vertex count does not match vertex count";
		return false;
	}

	return true;
}

void FStaticMeshData::BuildTriangleBVH()
{
	TArray<FMeshTriangleElement> Elements;
	for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
	{
		FMeshTriangleElement Element;
		Element.TriangleIndex = i / 3;
		FVector vertices[3];
		for (uint32 j = 0; j < 3; ++j)
		{
			uint32 index = Indices[i + j];
			vertices[j] = Vertices[index].Position;
		}
		Element.Bounds.Min.X = std::min({ vertices[0].X, vertices[1].X, vertices[2].X });
		Element.Bounds.Min.Y = std::min({ vertices[0].Y, vertices[1].Y, vertices[2].Y });
		Element.Bounds.Min.Z = std::min({ vertices[0].Z, vertices[1].Z, vertices[2].Z });
		Element.Bounds.Max.X = std::max({ vertices[0].X, vertices[1].X, vertices[2].X });
		Element.Bounds.Max.Y = std::max({ vertices[0].Y, vertices[1].Y, vertices[2].Y });
		Element.Bounds.Max.Z = std::max({ vertices[0].Z, vertices[1].Z, vertices[2].Z });
		Elements.Add(Element);
	}

	TriangleBVH.emplace([](const FMeshTriangleElement& Element) {
		return Element.Bounds;
	});

	TriangleBVH->Build(Elements);
}
