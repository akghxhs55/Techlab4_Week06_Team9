#include "EnginePCH.h"
#include "StaticMeshLODGenerator.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <cstdint>
#include <limits>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
    // 원본 LOD0의 삼각형 수에 곱한다.
    constexpr double TargetRatios[4] = {
        1.0, 0.75, 0.6, 0.3
    };

    struct FQuadric
    {
        double M[4][4]{};

        // 평면 n.x*x + n.y*y + n.z*z + d = 0의 오차를 더한다.
        // Q += [nx, ny, nz, d]^T * [nx, ny, nz, d]
        void AddPlane(const FVector& Normal, double D)
        {
            const double Plane[4] = {
                Normal.X, Normal.Y, Normal.Z, D
            };

            for (int I = 0; I < 4; ++I)
            {
                for (int J = 0; J < 4; ++J)
                {
                    M[I][J] += Plane[I] * Plane[J];
                }
            }
        }

        FQuadric& operator+=(const FQuadric& Other)
        {
            for (int I = 0; I < 4; ++I)
            {
                for (int J = 0; J < 4; ++J)
                {
                    M[I][J] += Other.M[I][J];
                }
            }
            return *this;
        }

        // 위치 p에서의 QEM 비용: [px, py, pz, 1] Q [px, py, pz, 1]^T
        double Evaluate(const FVector& Position) const
        {
            const double P[4] = {
                Position.X, Position.Y, Position.Z, 1.0
            };

            double Cost = 0.0;
            for (int I = 0; I < 4; ++I)
            {
                for (int J = 0; J < 4; ++J)
                {
                    Cost += P[I] * M[I][J] * P[J];
                }
            }
            return Cost;
        }
    };

    bool BuildVertexQuadrics(const FStaticMeshData& Mesh,std::vector<FQuadric>& OutQuadrics, FString& OutError)
    {
        OutQuadrics.assign(static_cast<size_t>(Mesh.Vertices.Num()), FQuadric{});

        for (uint32 Index = 0; Index < static_cast<uint32>(Mesh.Indices.Num()); Index += 3)
        {
            const uint32 IA = Mesh.Indices[Index];
            const uint32 IB = Mesh.Indices[Index + 1];
            const uint32 IC = Mesh.Indices[Index + 2];

            const FVector A = Mesh.Vertices[IA].Position;
            const FVector B = Mesh.Vertices[IB].Position;
            const FVector C = Mesh.Vertices[IC].Position;

            const FVector Cross = FVector::Cross(B - A, C - A);

            const float LengthSquared = Cross.Dot(Cross);
            if (LengthSquared <= 1e-20f)
            {
                OutError = "LOD0 contains a degenerate triangle";
                return false;
            }

            const FVector Normal = Cross * (1.0f / std::sqrt(LengthSquared));

            const double D = -static_cast<double>(Normal.Dot(A));

            // 이 면을 공유하는 세 정점에 같은 평면 오차를 더한다.
            OutQuadrics[IA].AddPlane(Normal, D);
            OutQuadrics[IB].AddPlane(Normal, D);
            OutQuadrics[IC].AddPlane(Normal, D);
        }
        return true;
    }

    struct FEdgePlacement
    {
        FVector Position;
        float T = 0.0f;  // A + T * (B - A)
        double Cost = 0.0;
    };

    FEdgePlacement FindBestPointOnEdge(const FVector& A,const FVector& B, const FQuadric& QA, const FQuadric& QB)
    {
        // 두 정점을 하나로 합쳤을 때의 오차 행렬
        FQuadric Q = QA;
        Q += QB;

        const FVector Direction = B - A;

        // P(t) = A + t(B-A), 0 <= t <= 1
        // E(t) = P(t)^T Q P(t)
        //      = Quadratic*t*t + Linear*t + Constant
        const double P0[4] = { A.X, A.Y, A.Z, 1.0 };
        const double Delta[4] = { Direction.X, Direction.Y, Direction.Z, 0.0 };

        double Quadratic = 0.0;
        double Linear = 0.0;

        for (int I = 0; I < 4; ++I)
        {
            for (int J = 0; J < 4; ++J)
            {
                Quadratic += Delta[I] * Q.M[I][J] * Delta[J];

                Linear += 2.0 * P0[I] * Q.M[I][J] * Delta[J];
            }
        }

        auto MakePlacement = [&](float T)
            {
                const FVector Position = A + Direction * T;
                return FEdgePlacement{ Position, T, Q.Evaluate(Position) };
            };

        // 양 끝점도 반드시 후보에 넣는다.
        FEdgePlacement Best = MakePlacement(0.0f);

        const FEdgePlacement End = MakePlacement(1.0f);
        if (End.Cost < Best.Cost) Best = End;

        // 이차 함수의 최소점이 있으면 edge 범위 [0,1]로 제한한다.
        if (Quadratic > 1e-20)
        {
            const double UnclampedT = -Linear / (2.0 * Quadratic);
            const float T = static_cast<float>(std::clamp(UnclampedT, 0.0, 1.0));
            const FEdgePlacement Middle = MakePlacement(T);

            if (Middle.Cost < Best.Cost) Best = Middle;
        }
        return Best;
    }

    struct FWorkTriangle
    {
        uint32 V[3]{};
        uint32 MaterialSlotIndex = 0;
        bool bAlive = true;
    };

    struct FWorkVertex
    {
        FVertexPNCT Vertex;
        FQuadric Quadric;

        // 고정된 특징점이 collapse의 도착점이 될 때 원본 위치 ID를 유지한다.
        // UV seam의 분리된 렌더 정점을 LOD 법선 계산에서 다시 찾는 데 쓴다.
        int32 SourcePositionIndex = -1;

        // 이 정점을 사용하는 삼각형 ID
        std::unordered_set<uint32> Faces;

        // 삼각형의 edge로 연결된 정점 ID
        std::unordered_set<uint32> Neighbors;

        uint32 Revision = 0;
        bool bProtected = false;
        bool bAlive = true;
    };

    struct FWorkMesh
    {
        std::vector<FWorkVertex> Vertices;
        std::vector<FWorkTriangle> Triangles;
        uint32 LiveTriangles = 0;
    };

    // LOD0을 작업용 자료구조로 복사
    bool BuildWorkMesh(const FStaticMeshData& Source, const std::vector<FQuadric>& Quadrics, FWorkMesh& Out, FString& OutError)
    {
        const uint32 VertexCount = static_cast<uint32>(Source.Vertices.Num());
        const uint32 TriangleCount = static_cast<uint32>(Source.Indices.Num() / 3);

        Out.Vertices.resize(VertexCount);
        Out.Triangles.resize(TriangleCount);
        Out.LiveTriangles = TriangleCount;

        for (uint32 I = 0; I < VertexCount; ++I)
        {
            Out.Vertices[I].Vertex = Source.Vertices[I];
            Out.Vertices[I].Quadric = Quadrics[I];
            Out.Vertices[I].SourcePositionIndex = Source.LODSourceVertices[I].PositionIndex;
        }

        // 원본 삼각형 하나가 어느 material section에 속하는지 기록한다.
        std::vector<int32> TriangleMaterials(TriangleCount, -1);

        for (const FStaticMeshSection& Section : Source.Sections)
        {
            if (Section.StartIndex % 3 != 0 ||
                Section.IndexCount % 3 != 0)
            {
                OutError = "Section range is not triangle-aligned";
                return false;
            }

            const uint32 End = Section.StartIndex + Section.IndexCount;

            for (uint32 Index = Section.StartIndex; Index < End; Index += 3)
            {
                const uint32 TriangleID = Index / 3;

                if (TriangleID >= TriangleCount || TriangleMaterials[TriangleID] != -1)
                {
                    OutError = "Sections overlap or exceed triangle range";
                    return false;
                }

                TriangleMaterials[TriangleID] = static_cast<int32>(Section.MaterialSlotIndex);
            }
        }

        for (uint32 TriangleID = 0; TriangleID < TriangleCount; ++TriangleID)
        {
            if (TriangleMaterials[TriangleID] < 0)
            {
                OutError = "A triangle has no material section";
                return false;
            }

            FWorkTriangle& Face = Out.Triangles[TriangleID];
            Face.MaterialSlotIndex = static_cast<uint32>(TriangleMaterials[TriangleID]);

            for (uint32 Corner = 0; Corner < 3; ++Corner)
            {
                Face.V[Corner] = Source.Indices[TriangleID * 3 + Corner];
                Out.Vertices[Face.V[Corner]].Faces.insert(TriangleID);
            }

            // 세 edge에 대한 이웃 관계를 양방향으로 기록한다.
            for (uint32 Edge = 0; Edge < 3; ++Edge)
            {
                const uint32 A = Face.V[Edge];
                const uint32 B = Face.V[(Edge + 1) % 3];

                Out.Vertices[A].Neighbors.insert(B);
                Out.Vertices[B].Neighbors.insert(A);
            }
        }
        return true;
    }

    uint64 MakeGeometricEdgeKey(uint32 A, uint32 B)
    {
        if (A > B) std::swap(A, B);

        return (uint64(A) << 32) | uint64(B);
    }

    struct FEdgeUse
    {
        uint32 TriangleID = 0;
        uint32 RenderA = 0;
        uint32 RenderB = 0;
    };

    FVector GetFaceNormal(const FWorkTriangle& Face,const FWorkMesh& Work)
    {
        const FVector& A = Work.Vertices[Face.V[0]].Vertex.Position;
        const FVector& B = Work.Vertices[Face.V[1]].Vertex.Position;
        const FVector& C = Work.Vertices[Face.V[2]].Vertex.Position;

        return FVector::Cross(B - A, C - A).Normalized();
    }

    const FLODSourceVertex& SourceEndpoint( const FEdgeUse& Use, uint32 PositionID, const FStaticMeshData& Source)
    {
        const FLODSourceVertex& A = Source.LODSourceVertices[Use.RenderA];

        if (static_cast<uint32>(A.PositionIndex) == PositionID) return A;

        return Source.LODSourceVertices[Use.RenderB];
    }

    void MarkProtectedVertices(const FStaticMeshData& Source, FWorkMesh& Work, FLODGenerateResult& Result)
    {
        std::unordered_map<uint64, std::vector<FEdgeUse>> EdgeUses;

        // 각 삼각형의 기하 edge 3개를 수집한다.
        for (uint32 TriangleID = 0; TriangleID < Work.Triangles.size(); ++TriangleID)
        {
            const FWorkTriangle& Face = Work.Triangles[TriangleID];

            for (uint32 Edge = 0; Edge < 3; ++Edge)
            {
                const uint32 RenderA = Face.V[Edge];
                const uint32 RenderB = Face.V[(Edge + 1) % 3];

                const int32 PositionA = Source.LODSourceVertices[RenderA].PositionIndex;
                const int32 PositionB = Source.LODSourceVertices[RenderB].PositionIndex;

                // 원본 topology ID가 없는 입력은 2~3단계의
                // Generate() 입력 검사에서 미리 거부해야 한다.
                if (PositionA < 0 || PositionB < 0) continue;

                const uint64 Key = MakeGeometricEdgeKey(static_cast<uint32>(PositionA),static_cast<uint32>(PositionB));

                EdgeUses[Key].push_back({TriangleID, RenderA, RenderB});
            }
        }

        // 초기값: 60도 이상 꺾이면 hard edge.
        constexpr float HardEdgeCosine = 0.6f;

        for (const auto& [Key, Uses] : EdgeUses)
        {
            bool bProtect = Uses.size() != 2;

            if (!bProtect)
            {
                const FEdgeUse& U0 = Uses[0];
                const FEdgeUse& U1 = Uses[1];

                const FWorkTriangle& F0 = Work.Triangles[U0.TriangleID];
                const FWorkTriangle& F1 = Work.Triangles[U1.TriangleID];

                const uint32 PositionA = static_cast<uint32>(Key >> 32);
                const uint32 PositionB = static_cast<uint32>(Key & 0xffffffffu);

                const auto& A0 = SourceEndpoint(U0, PositionA, Source);
                const auto& A1 = SourceEndpoint(U1, PositionA, Source);
                const auto& B0 = SourceEndpoint(U0, PositionB, Source);
                const auto& B1 = SourceEndpoint(U1, PositionB, Source);

                const bool bMaterialBoundary = F0.MaterialSlotIndex != F1.MaterialSlotIndex;
                const bool bUVSeam = A0.UVIndex != A1.UVIndex || B0.UVIndex != B1.UVIndex;

                // OBJ의 authored normal이 서로 다르면 보수적으로 보호.
                // normal이 아예 없는 OBJ는 아래 면 각도로 판단한다.
                const bool bNormalDiscontinuity =
                    (A0.NormalIndex >= 0 &&
                        A1.NormalIndex >= 0 &&
                        A0.NormalIndex != A1.NormalIndex) ||
                    (B0.NormalIndex >= 0 &&
                        B1.NormalIndex >= 0 &&
                        B0.NormalIndex != B1.NormalIndex);

                const float FaceDot = GetFaceNormal(F0, Work).Dot(GetFaceNormal(F1, Work));
                const bool bHardEdge = FaceDot < HardEdgeCosine || bNormalDiscontinuity;
                bProtect = bMaterialBoundary || bUVSeam || bHardEdge;
            }

            if (!bProtect) continue;

            ++Result.RejectedFeatureEdges;

            // edge 양쪽에서 사용된 렌더 정점 모두 고정한다.
            for (const FEdgeUse& Use : Uses)
            {
                Work.Vertices[Use.RenderA].bProtected = true;
                Work.Vertices[Use.RenderB].bProtected = true;
            }
        }
    }

    struct FCollapseCandidate
    {
        uint32 A = 0;
        uint32 B = 0;
        uint32 RevisionA = 0;
        uint32 RevisionB = 0;
        FEdgePlacement Placement;
    };

    struct FGreaterCollapseCost
    {
        bool operator()(const FCollapseCandidate& L,const FCollapseCandidate& R) const
        {
            if (L.Placement.Cost != R.Placement.Cost)
                return L.Placement.Cost > R.Placement.Cost;

            // 같은 비용의 처리 순서를 고정한다.
            return std::tie(L.A, L.B) > std::tie(R.A, R.B);
        }
    };

    using FCollapseHeap = std::priority_queue<FCollapseCandidate, std::vector<FCollapseCandidate>, FGreaterCollapseCost>;

    void PushCandidate(uint32 A, uint32 B, const FWorkMesh& Work, FCollapseHeap& Heap)
    {
        if (A > B) std::swap(A, B);

        const FWorkVertex& VA = Work.Vertices[A];
        const FWorkVertex& VB = Work.Vertices[B];

        if (!VA.bAlive || !VB.bAlive || (VA.bProtected && VB.bProtected)) return;
        if (!VA.Neighbors.contains(B)) return;

        FEdgePlacement Placement;
        if (VA.bProtected || VB.bProtected)
        {
            const bool bPinA = VA.bProtected;
            const FVector PinnedPosition =  bPinA ? VA.Vertex.Position : VB.Vertex.Position;

            FQuadric Combined = VA.Quadric;
            Combined += VB.Quadric;

            Placement = {
                PinnedPosition,
                bPinA ? 0.0f : 1.0f,
                Combined.Evaluate(PinnedPosition)
            };
        }
        else
        {
            Placement = FindBestPointOnEdge(
                VA.Vertex.Position, VB.Vertex.Position,
                VA.Quadric, VB.Quadric);
        }

        if (!std::isfinite(Placement.Cost))return;

        Heap.push({A, B, VA.Revision, VB.Revision, Placement});
    }

    std::vector<uint32> SharedFaces(uint32 A, uint32 B, const FWorkMesh& Work)
    {
        std::vector<uint32> Result;
        for (uint32 FaceID : Work.Vertices[A].Faces)
        {
            if (Work.Triangles[FaceID].bAlive && Work.Vertices[B].Faces.contains(FaceID))
            {
                Result.push_back(FaceID);
            }
        }
        return Result;
    }

    bool PassesLinkCondition(uint32 A,uint32 B, const FWorkMesh& Work)
    {
        const std::vector<uint32> Shared = SharedFaces(A, B, Work);

        if (Shared.size() != 2) return false;

        std::unordered_set<uint32> OppositeVertices;

        for (uint32 FaceID : Shared)
        {
            const FWorkTriangle& Face = Work.Triangles[FaceID];

            for (uint32 V : Face.V)
            {
                if (V != A && V != B) OppositeVertices.insert(V);
            }
        }

        if (OppositeVertices.size() != 2) return false;

        std::unordered_set<uint32> CommonNeighbors;

        for (uint32 Neighbor : Work.Vertices[A].Neighbors)
        {
            if (Work.Vertices[B].Neighbors.contains(Neighbor))
            {
                CommonNeighbors.insert(Neighbor);
            }
        }
        return CommonNeighbors == OppositeVertices;
    }

    enum class ECollapseCheck
    {
        Allowed,
        Topology,
        Flip
    };

    ECollapseCheck CheckCollapse(const FCollapseCandidate& Candidate, const FWorkMesh& Work)
    {
        const uint32 A = Candidate.A;
        const uint32 B = Candidate.B;

        if (!PassesLinkCondition(A, B, Work)) return ECollapseCheck::Topology;

        std::unordered_set<uint32> IncidentFaces = Work.Vertices[A].Faces;

        IncidentFaces.insert(Work.Vertices[B].Faces.begin(), Work.Vertices[B].Faces.end());

        for (uint32 FaceID : IncidentFaces)
        {
            const FWorkTriangle& Face = Work.Triangles[FaceID];

            if (!Face.bAlive) continue;

            const bool bHasA = Face.V[0] == A || Face.V[1] == A || Face.V[2] == A;
            const bool bHasB = Face.V[0] == B || Face.V[1] == B || Face.V[2] == B;

            // A-B를 공유하는 두 면은 제거되므로 검사하지 않는다.
            if (bHasA && bHasB) continue;

            FVector OldPositions[3];
            FVector NewPositions[3];

            for (int I = 0; I < 3; ++I)
            {
                const uint32 V = Face.V[I];
                OldPositions[I] = Work.Vertices[V].Vertex.Position;

                NewPositions[I] = (V == A || V == B)
                    ? Candidate.Placement.Position : OldPositions[I];
            }

            const FVector OldCross = FVector::Cross(
                OldPositions[1] - OldPositions[0],
                OldPositions[2] - OldPositions[0]);

            const FVector NewCross = FVector::Cross(
                NewPositions[1] - NewPositions[0],
                NewPositions[2] - NewPositions[0]);

            if (NewCross.Dot(NewCross) <= 1e-20f ||
                OldCross.Dot(NewCross) <= 0.0f)
            {
                return ECollapseCheck::Flip;
            }
        }

        return ECollapseCheck::Allowed;
    }

    void ApplyCollapse(const FCollapseCandidate& Candidate, FWorkMesh& Work, FCollapseHeap& Heap)
    {
        const uint32 A = Candidate.A;
        const uint32 B = Candidate.B;
        const float T = Candidate.Placement.T;

        FWorkVertex& VA = Work.Vertices[A];
        FWorkVertex& VB = Work.Vertices[B];

        // 변경 전 이웃을 기억한다. 변경 후 이 영역의 후보만 갱신.
        std::unordered_set<uint32> Touched{A, B};
        Touched.insert(VA.Neighbors.begin(), VA.Neighbors.end());
        Touched.insert(VB.Neighbors.begin(), VB.Neighbors.end());

        const FVertexPNCT OldA = VA.Vertex;
        const FVertexPNCT OldB = VB.Vertex;

        if (VB.bProtected)
        {
            // A가 B 위치에 고정되면 seam과 원래의 smoothing 정보도 B를 따른다.
            VA.SourcePositionIndex = VB.SourcePositionIndex;
            VA.Vertex.Normal = OldB.Normal;
        }
        else if (!VA.bProtected)
        {
            // 이동한 내부 정점은 더 이상 원본 위치의 seam 정점이 아니다.
            VA.SourcePositionIndex = -1;
        }

        VA.Vertex.Position = Candidate.Placement.Position;
        VA.Vertex.UV = OldA.UV * (1.0f - T) + OldB.UV * T;
        VA.Vertex.Color = OldA.Color * (1.0f - T) + OldB.Color * T;

        // Normal은 결과 LOD를 만들 때 재계산한다.
        VA.Quadric += VB.Quadric;
        VA.bProtected = VA.bProtected || VB.bProtected;

        // 순회 도중 VB.Faces를 수정하므로 목록을 복사한다.
        const std::vector<uint32> BFaces(VB.Faces.begin(), VB.Faces.end());

        for (uint32 FaceID : BFaces)
        {
            FWorkTriangle& Face = Work.Triangles[FaceID];

            if (!Face.bAlive) continue;

            bool bContainsA = false;
            for (uint32 V : Face.V) 
                bContainsA |= V == A;

            if (bContainsA)
            {
                // Collapse로 퇴화하는 두 면을 제거한다.
                Face.bAlive = false;
                --Work.LiveTriangles;

                for (uint32 V : Face.V)
                    Work.Vertices[V].Faces.erase(FaceID);
            }
            else
            {
                // 살아남는 면에서 B → A 치환
                for (uint32& V : Face.V)
                {
                    if (V == B) V = A;
                }

                VA.Faces.insert(FaceID);
                VB.Faces.erase(FaceID);
            }
        }

        VB.Faces.clear();
        VB.Neighbors.clear();
        VB.bAlive = false;

        // Touched 정점의 이웃 관계를 살아 있는 면에서 재구성.
        for (uint32 V : Touched)
        {
            FWorkVertex& Vertex = Work.Vertices[V];
            Vertex.Neighbors.clear();

            if (!Vertex.bAlive)
                continue;

            for (uint32 FaceID : Vertex.Faces)
            {
                const FWorkTriangle& Face =
                    Work.Triangles[FaceID];

                if (!Face.bAlive)
                    continue;

                for (uint32 Other : Face.V)
                {
                    if (Other != V)
                        Vertex.Neighbors.insert(Other);
                }
            }

            ++Vertex.Revision;
        }

        // 이전 힙 항목은 Revision 불일치로 폐기된다.
        // 현재 주변의 edge만 새 비용으로 다시 넣는다.
        for (uint32 V : Touched)
        {
            if (!Work.Vertices[V].bAlive)
                continue;

            for (uint32 Neighbor :
            Work.Vertices[V].Neighbors)
            {
                PushCandidate(V, Neighbor, Work, Heap);
            }
        }
    }

    FStaticMeshData BuildLODMesh(const FWorkMesh& Work,const FStaticMeshData& LOD0)
    {
        FStaticMeshData Out;
        Out.MaterialSlots = LOD0.MaterialSlots;

        constexpr uint32 Invalid = std::numeric_limits<uint32>::max();

        std::vector<uint32> Remap(Work.Vertices.size(), Invalid);

        // 재질별로 인덱스를 모아 Section을 연속 범위로 만든다.
        for (uint32 Slot = 0; Slot < static_cast<uint32>(Out.MaterialSlots.Num()); ++Slot)
        {
            FStaticMeshSection Section;
            Section.StartIndex = static_cast<uint32>(Out.Indices.Num());
            Section.MaterialSlotIndex = Slot;

            for (const FWorkTriangle& Face : Work.Triangles)
            {
                if (!Face.bAlive || Face.MaterialSlotIndex != Slot)
                    continue;

                for (uint32 OldIndex : Face.V)
                {
                    if (Remap[OldIndex] == Invalid)
                    {
                        Remap[OldIndex] = static_cast<uint32>(Out.Vertices.Num());
                        Out.Vertices.Add(Work.Vertices[OldIndex].Vertex);
                    }

                    Out.Indices.Add(Remap[OldIndex]);
                }
            }

            Section.IndexCount = static_cast<uint32>(Out.Indices.Num()) - Section.StartIndex;
            if (Section.IndexCount > 0) Out.Sections.Add(Section);
        }

        // 면적 가중 normal을 계산한다. UV seam에서는 같은 원본 위치에
        // 여러 렌더 정점이 있으므로, 원본에서 매끈했던 정점끼리만 합친다.
        std::vector<FVector> NormalSums(Out.Vertices.Num(), FVector());

        for (uint32 I = 0;I < static_cast<uint32>( Out.Indices.Num());I += 3)
        {
            const uint32 IA = Out.Indices[I];
            const uint32 IB = Out.Indices[I + 1];
            const uint32 IC = Out.Indices[I + 2];

            const FVector& A = Out.Vertices[IA].Position;
            const FVector& B = Out.Vertices[IB].Position;
            const FVector& C = Out.Vertices[IC].Position;

            const FVector Cross = FVector::Cross(B - A, C - A);

            NormalSums[IA] += Cross;
            NormalSums[IB] += Cross;
            NormalSums[IC] += Cross;
        }

        std::unordered_map<int32, std::vector<uint32>> VerticesBySourcePosition;
        for (uint32 OldIndex = 0; OldIndex < Remap.size(); ++OldIndex)
        {
            if (Remap[OldIndex] == Invalid) continue;

            const int32 PositionIndex = Work.Vertices[OldIndex].SourcePositionIndex;
            if (PositionIndex >= 0)
                VerticesBySourcePosition[PositionIndex].push_back(OldIndex);
        }

        // 원본에서 다른 방향을 향하던 법선은 hard edge로 취급한다.
        // 같은 위치라도 collapse 결과가 벌어졌다면 합치지 않는다.
        constexpr float SmoothNormalCosine = 0.9999f;
        constexpr float SamePositionDistanceSquared = 1e-12f;

        for (const auto& [PositionIndex, OldIndices] : VerticesBySourcePosition)
        {
            if (OldIndices.size() < 2) continue;

            std::vector<bool> Used(OldIndices.size(), false);
            for (size_t I = 0; I < OldIndices.size(); ++I)
            {
                if (Used[I]) continue;

                Used[I] = true;
                const uint32 AnchorOld = OldIndices[I];
                const uint32 AnchorNew = Remap[AnchorOld];
                const FVector AnchorNormal = Work.Vertices[AnchorOld].Vertex.Normal.Normalized();
                const FVector AnchorPosition = Out.Vertices[AnchorNew].Position;

                FVector Sum = NormalSums[AnchorNew];
                std::vector<uint32> Group{AnchorNew};

                for (size_t J = I + 1; J < OldIndices.size(); ++J)
                {
                    if (Used[J]) continue;

                    const uint32 OtherOld = OldIndices[J];
                    const uint32 OtherNew = Remap[OtherOld];
                    const FVector OtherNormal = Work.Vertices[OtherOld].Vertex.Normal.Normalized();
                    const FVector Delta = Out.Vertices[OtherNew].Position - AnchorPosition;

                    if (AnchorNormal.Dot(OtherNormal) < SmoothNormalCosine ||
                        Delta.Dot(Delta) > SamePositionDistanceSquared)
                    {
                        continue;
                    }

                    Used[J] = true;
                    Sum += NormalSums[OtherNew];
                    Group.push_back(OtherNew);
                }

                if (Group.size() > 1)
                {
                    for (uint32 Index : Group)
                        NormalSums[Index] = Sum;
                }
            }
        }

        for (uint32 I = 0;I < static_cast<uint32>( Out.Vertices.Num()); ++I)
        {
            Out.Vertices[I].Normal = NormalSums[I].Normalized();
        }

        FVector Min = Out.Vertices[0].Position;
        FVector Max = Min;

        for (const FVertexPNCT& Vertex : Out.Vertices)
        {
            Min.X = std::min(Min.X, Vertex.Position.X);
            Min.Y = std::min(Min.Y, Vertex.Position.Y);
            Min.Z = std::min(Min.Z, Vertex.Position.Z);

            Max.X = std::max(Max.X, Vertex.Position.X);
            Max.Y = std::max(Max.Y, Vertex.Position.Y);
            Max.Z = std::max(Max.Z, Vertex.Position.Z);
        }

        Out.AABB = { Min, Max };

        // LOD1~3을 다시 단순화하지 않는다.
        // 다음 Generate 호출도 항상 LOD0에서 시작한다.
        return Out;
    }
}

FLODGenerateResult FStaticMeshLODGenerator::Generate(const FStaticMeshData& LOD0, TArray<FStaticMeshData>& OutLODs)
{
    OutLODs.Reset();
    FLODGenerateResult Result;

    FString ValidationError;
    if (!LOD0.Validate(ValidationError))
    {
        Result.FailureReason = ValidationError;
        return Result;
    }

    if (LOD0.LODSourceVertices.Num() != LOD0.Vertices.Num())
    {
        Result.FailureReason = "LOD0 has no source topology data";
        return Result;
    }

    const uint32 BaseTriangles = static_cast<uint32>(LOD0.Indices.Num() / 3);
    Result.ActualTriangles[0] = BaseTriangles;

    for (uint32 LOD = 0; LOD < 4; ++LOD)
    {
        Result.TargetTriangles[LOD] = static_cast<uint32>(std::floor(BaseTriangles * TargetRatios[LOD]));
    }

    std::vector<FQuadric> Quadrics;
    if (!BuildVertexQuadrics(LOD0, Quadrics, Result.FailureReason))
    {
        return Result;
    }

    FWorkMesh Work;
    if (!BuildWorkMesh(LOD0, Quadrics, Work, Result.FailureReason))
    {
        return Result;
    }

    MarkProtectedVertices(LOD0, Work, Result);

    FCollapseHeap Heap;

    for (uint32 A = 0; A < Work.Vertices.size(); ++A)
    {
        for (uint32 B : Work.Vertices[A].Neighbors)
        {
            if (A < B) PushCandidate(A, B, Work, Heap);
        }
    }

    uint32 NextLOD = 1;

    while (NextLOD <= 3 && !Heap.empty())
    {
        const FCollapseCandidate Candidate = Heap.top();
        Heap.pop();

        const FWorkVertex& VA = Work.Vertices[Candidate.A];
        const FWorkVertex& VB = Work.Vertices[Candidate.B];

        // ApplyCollapse() 전 상태에서 계산한 오래된 후보는 버린다.
        if (!VA.bAlive || !VB.bAlive || VA.Revision != Candidate.RevisionA ||
            VB.Revision != Candidate.RevisionB)
        {
            continue;
        }

        const ECollapseCheck Check = CheckCollapse(Candidate, Work);

        if (Check == ECollapseCheck::Topology)
        {
            ++Result.RejectedTopology;
            continue;
        }

        if (Check == ECollapseCheck::Flip)
        {
            ++Result.RejectedFlips;
            continue;
        }

        ApplyCollapse(Candidate, Work, Heap);

        // Collapse 하나가 목표를 여러 개 통과할 수도 있으므로 while.
        while (NextLOD <= 3 && Work.LiveTriangles <= Result.TargetTriangles[NextLOD])
        {
            FStaticMeshData Snapshot = BuildLODMesh(Work, LOD0);

            FString Error;
            if (!Snapshot.Validate(Error))
            {
                Result.FailureReason = Error;
                OutLODs.Reset();
                return Result;
            }

            Result.ActualTriangles[NextLOD] = Work.LiveTriangles;

            OutLODs.Add(std::move(Snapshot));

            ++NextLOD;
        }
    }

    if (NextLOD != 4)
    {
        // 마지막 성공 LOD보다 더 줄어든 상태라면 그 결과도 저장한다.
        if (Work.LiveTriangles > 0 &&
            Work.LiveTriangles < Result.ActualTriangles[NextLOD - 1])
        {
            FStaticMeshData Snapshot = BuildLODMesh(Work, LOD0);
            FString Error;
            if (!Snapshot.Validate(Error))
            {
                Result.FailureReason = Error;
                OutLODs.Reset();
                return Result;
            }

            Result.ActualTriangles[NextLOD] = Work.LiveTriangles;
            OutLODs.Add(std::move(Snapshot));
            ++NextLOD;
        }

        // 더 줄일 수 없으면 직전 메시로 남은 슬롯을 채운다.
        while (NextLOD <= 3)
        {
            FStaticMeshData Snapshot =
                NextLOD == 1 ? LOD0 : OutLODs.Last();

            Result.ActualTriangles[NextLOD] =
                Result.ActualTriangles[NextLOD - 1];
            OutLODs.Add(std::move(Snapshot));
            ++NextLOD;
        }

        Result.bSuccess = true;
        Result.FailureReason = "Fixed target not reached; best effort LOD used";
        return Result;
    }

    Result.bSuccess = true;
    return Result;
}
