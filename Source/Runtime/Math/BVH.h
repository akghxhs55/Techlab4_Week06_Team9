#pragma once

#include "FBox.h"

template <typename T>
class TBVH
{
public:
	using FBoundsGetter = std::function<FBox(T const&)>;

	explicit TBVH(FBoundsGetter BoundsGetter) : BoundsGetter(std::move(BoundsGetter)) {}

	void Build(std::span<T const> InElements);

	// 루트노드부터 Bounding Box 재계산
	void Refit();

	template <typename TBoundsPredicate, typename TVisitor>
	void Query(TBoundsPredicate&& BoundsTest, TVisitor&& Visitor) const;

	void Clear();

private:
	struct FNode
	{
		FBox Bounds;

		uint32 Parent = InvalidIndex;
		uint32 LeftChild = InvalidIndex;
		uint32 RightChild = InvalidIndex;

		uint32 FirstElement = 0;
		uint32 ElementCount = 0;

		bool IsLeaf() const
		{
			return LeftChild == InvalidIndex;
		}
	};

	struct FElement
	{
		T Value;
		FBox Bounds;
	};

	struct FSplit
	{
		int32 Axis = -1;
		float Position = 0.0f;
		float Cost = std::numeric_limits<float>::max();
	};

	static constexpr uint32 InvalidIndex = std::numeric_limits<uint32>::max();
	static constexpr uint32 MaxDepth = 32;
	static constexpr uint32 MinSplitSize = 8;
	static constexpr uint32 BinCount = 16;

	uint32 BuildNode(uint32 First, uint32 Count, uint32 Depth);
	FBox ComputeBounds(uint32 First, uint32 Count) const;
	FSplit FindBestSplit(uint32 First, uint32 Count, const FBox& Bounds) const;
	uint32 Partition(uint32 First, uint32 Count, int32 Axis, float Position);

	FBox RefitNode(uint32 Index);

	static float SurfaceArea(const FBox& Box);

	FBoundsGetter BoundsGetter;

	TArray<FNode> Nodes;
	TArray<FElement> Elements;
};

template <typename T>
void TBVH<T>::Build(std::span<T const> InElements)
{
	Clear();
	if (InElements.empty())
	{
		return;
	}

	Elements.Reserve(InElements.size());
	for (const T& Element : InElements)
	{
		Elements.Emplace(Element, BoundsGetter(Element));
	}

	BuildNode(0, static_cast<uint32>(Elements.size()), 0);
}

template <typename T>
void TBVH<T>::Refit()
{
	if (Nodes.IsEmpty())
	{
		return;
	}
	RefitNode(0);
}

template <typename T>
template <typename TBoundsPredicate, typename TVisitor>
void TBVH<T>::Query(TBoundsPredicate&& BoundsTest, TVisitor&& Visitor) const
{
	if (Nodes.IsEmpty())
	{
		return;
	}

	TArray<uint32> Stack;
	Stack.Add(0);

	while (!Stack.IsEmpty())
	{
		const uint32 Index = Stack.Last();
		Stack.RemoveLast();

		const FNode& Node = Nodes[Index];

		if (!BoundsTest(Node.Bounds))
		{
			continue;
		}

		if (Node.IsLeaf())
		{
			for (uint32 i = 0; i < Node.ElementCount; ++i)
			{
				const FElement& Element = Elements[Node.FirstElement + i];
				if (BoundsTest(Element.Bounds))
				{
					Visitor(Element.Value);
				}
			}
		}
		else
		{
			if (Node.LeftChild != InvalidIndex)
			{
				Stack.Add(Node.LeftChild);
			}
			if (Node.RightChild != InvalidIndex)
			{
				Stack.Add(Node.RightChild);
			}
		}
	}
}

template <typename T>
void TBVH<T>::Clear()
{
	Nodes.Reset();
	Elements.Reset();
}

template <typename T>
uint32 TBVH<T>::BuildNode(uint32 First, uint32 Count, uint32 Depth)
{
	const uint32 Index = static_cast<uint32>(Nodes.Num());
	Nodes.Emplace();

	FNode& Node = Nodes[Index];

	Node.FirstElement = First;
	Node.ElementCount = Count;
	Node.Bounds = ComputeBounds(First, Count);

	if (Count <= MinSplitSize || Depth >= MaxDepth)
	{
		return Index;
	}

	const FSplit Split = FindBestSplit(First, Count, Node.Bounds);
	if (Split.Axis == -1 || Split.Cost >= static_cast<float>(Count)) // 분할 후의 비용이 더 크다고 보이는 경우
	{
		return Index;
	}

	const uint32 Middle = Partition(First, Count, Split.Axis, Split.Position);
	if (Middle == First || Middle == First + Count)
	{
		return Index;
	}

	const uint32 LeftChild = BuildNode(First, Middle - First, Depth + 1);
	const uint32 RightChild = BuildNode(Middle, First + Count - Middle, Depth + 1);
	if (LeftChild != InvalidIndex)
	{
		Nodes[LeftChild].Parent = Index;
	}
	if (RightChild != InvalidIndex)
	{
		Nodes[RightChild].Parent = Index;
	}

	FNode& FinalNode = Nodes[Index]; // Nodes가 변경되어 참조가 무효화되는 경우 방ㅂ지
	FinalNode.LeftChild = LeftChild;
	FinalNode.RightChild = RightChild;
	FinalNode.ElementCount = 0;

	return Index;
}

template <typename T>
FBox TBVH<T>::ComputeBounds(uint32 First, uint32 Count) const
{
	FBox Result{ FVector{ std::numeric_limits<float>::max() }, FVector{ std::numeric_limits<float>::lowest() } };
	for (uint32 i = First; i < First + Count; ++i)
	{
		const FElement& Element = Elements[i];
		
		Result.Min.X = std::min(Result.Min.X, Element.Bounds.Min.X);
		Result.Min.Y = std::min(Result.Min.Y, Element.Bounds.Min.Y);
		Result.Min.Z = std::min(Result.Min.Z, Element.Bounds.Min.Z);

		Result.Max.X = std::max(Result.Max.X, Element.Bounds.Max.X);
		Result.Max.Y = std::max(Result.Max.Y, Element.Bounds.Max.Y);
		Result.Max.Z = std::max(Result.Max.Z, Element.Bounds.Max.Z);
	}
	return Result;
}

// SAH(Surface Area Heuristic)
template <typename T>
typename TBVH<T>::FSplit TBVH<T>::FindBestSplit(uint32 First, uint32 Count, const FBox& Bounds) const
{
	const float ParentArea = SurfaceArea(Bounds);
	if (ParentArea <= std::numeric_limits<float>::epsilon())
	{
		return FSplit{};
	}

	FSplit BestSplit{};
	for (uint32 Axis = 0; Axis < 3; ++Axis)
	{
		float MinCenter = std::numeric_limits<float>::max();
		float MaxCenter = std::numeric_limits<float>::lowest();
		for (uint32 i = First; i < First + Count; ++i)
		{
			const FElement& Element = Elements[i];
			float Center = (Element.Bounds.Min[Axis] + Element.Bounds.Max[Axis]) * 0.5f;

			MinCenter = std::min(MinCenter, Center);
			MaxCenter = std::max(MaxCenter, Center);
		}

		float BinWidth = (MaxCenter - MinCenter) / static_cast<float>(BinCount);
		if (BinWidth <= std::numeric_limits<float>::epsilon())
		{
			continue;
		}

		std::array<uint32, BinCount - 1> LeftCounts{};
		std::array<FBox, BinCount - 1> LeftBounds{};
		std::array<uint32, BinCount - 1> RightCounts{};
		std::array<FBox, BinCount - 1> RightBounds{};
		for (uint32 i = 0; i < BinCount - 1; ++i)
		{
			LeftBounds[i] = FBox{ FVector{ std::numeric_limits<float>::max() }, FVector{ std::numeric_limits<float>::lowest() } };
			RightBounds[i] = FBox{ FVector{ std::numeric_limits<float>::max() }, FVector{ std::numeric_limits<float>::lowest() } };
		}

		for (uint32 i = First; i < First + Count; ++i)
		{
			const FElement& Element = Elements[i];
			float Center = (Element.Bounds.Min[Axis] + Element.Bounds.Max[Axis]) * 0.5f;
			uint32 BinIndex = static_cast<uint32>((Center - MinCenter) / BinWidth);
			BinIndex = std::clamp(BinIndex, 0u, BinCount - 1);
			if (BinIndex < BinCount - 1)
			{
				LeftBounds[BinIndex].Expand(Element.Bounds);
				++LeftCounts[BinIndex];
			}
			if (BinIndex > 0)
			{
				RightBounds[BinIndex - 1].Expand(Element.Bounds);
				++RightCounts[BinIndex - 1];
			}
		}

		for (uint32 i = 1; i < BinCount - 1; ++i)
		{
			LeftCounts[i] += LeftCounts[i - 1];
			LeftBounds[i].Expand(LeftBounds[i - 1]);
			RightCounts[BinCount - 2 - i] += RightCounts[BinCount - 1 - i];
			RightBounds[BinCount - 2 - i].Expand(RightBounds[BinCount - 1 - i]);
		}

		for (uint32 i = 1; i < BinCount; ++i)
		{
			if (LeftCounts[i - 1] == 0 || RightCounts[i - 1] == 0)
			{
				continue;
			}

			float LeftArea = SurfaceArea(LeftBounds[i - 1]);
			float RightArea = SurfaceArea(RightBounds[i - 1]);
			float Cost = 1.0f + (LeftArea * static_cast<float>(LeftCounts[i - 1]) + RightArea * static_cast<float>(RightCounts[i - 1])) / ParentArea;
			if (Cost < BestSplit.Cost)
			{
				BestSplit.Axis = static_cast<int32>(Axis);
				BestSplit.Position = MinCenter + BinWidth * static_cast<float>(i);
				BestSplit.Cost = Cost;
			}
		}
	}

	return BestSplit;
}

template <typename T>
uint32 TBVH<T>::Partition(uint32 First, uint32 Count, int32 Axis, float Position)
{
	auto Begin = Elements.begin() + First;
	auto End = Begin + Count;

	auto Middle = std::partition(Begin, End, [Axis, Position](const FElement& Element) {
		float Center = (Element.Bounds.Min[Axis] + Element.Bounds.Max[Axis]) * 0.5f;
		return Center < Position;
	});

	return static_cast<uint32>(Middle - Elements.begin());
}

template <typename T>
FBox TBVH<T>::RefitNode(uint32 Index)
{
	FNode& Node = Nodes[Index];

	if (Node.IsLeaf())
	{
		FBox Bounds = FBox{FVector{std::numeric_limits<float>::max()}, FVector{std::numeric_limits<float>::lowest()}};

		for (uint32 i = 0; i < Node.ElementCount; ++i)
		{
			FElement& Element = Elements[Node.FirstElement + i];
			Element.Bounds = BoundsGetter(Element.Value);
			Bounds.Expand(Element.Bounds);
		}
		Node.Bounds = Bounds;
		return Bounds;
	}
	
	Node.Bounds = FBox{ FVector{std::numeric_limits<float>::max()}, FVector{std::numeric_limits<float>::lowest()} };
	if (Node.LeftChild != InvalidIndex)
	{
		Node.Bounds.Expand(RefitNode(Node.LeftChild));
	}
	if (Node.RightChild != InvalidIndex)
	{
		Node.Bounds.Expand(RefitNode(Node.RightChild));
	}

	return Node.Bounds;
}

template <typename T>
float TBVH<T>::SurfaceArea(const FBox& Box)
{
	const FVector Size = Box.Max - Box.Min;
	return 2.0f * (Size.X * Size.Y + Size.Y * Size.Z + Size.Z * Size.X);
}
