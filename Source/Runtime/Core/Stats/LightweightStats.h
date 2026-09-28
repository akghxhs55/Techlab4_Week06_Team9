#pragma once

#include "Core/Types.h"
#include "Core/Windows/WindowsPlatformTime.h"
#include "Container/Map.h"



struct TStatId
{
	const char* StatString = nullptr;

	constexpr TStatId() = default;
	constexpr TStatId(const char* InString)
		: StatString(InString) {
	}
	constexpr const char* GetName() const { return StatString; }
	constexpr bool IsValidStat() const { return StatString != nullptr; }
	constexpr bool operator==(TStatId Other) const { return StatString == Other.StatString; }
	constexpr bool operator!=(TStatId Other) const { return StatString != Other.StatString; }
};

// 스탯 하나의 누적 결과. 시간은 사이클로 모으고 표시할 때만 ms로 바꾼다.
struct FCycleStatData
{
	uint64 LastCycles = 0;
	uint64 TotalCycles = 0;
	uint32 CallCount = 0;

	double GetLastMs() const { return FPlatformTime::ToMilliseconds64(LastCycles); }
	double GetTotalMs() const { return FPlatformTime::ToMilliseconds64(TotalCycles); }
	double GetAverageMs() const { return CallCount > 0 ? GetTotalMs() / CallCount : 0.0; }
};

struct FStatHistory
{
	static constexpr uint32 Duration = 120;

	uint64 FrameCycles[Duration] = {};
	uint64 WindowSumCycles = 0;
	uint64 MaxFrameCycles = 0;

	uint32 NextIndex = 0;
	uint32 SampleCount = 0;

	void PushFrame(uint64 Cycles)
	{
		WindowSumCycles -= FrameCycles[NextIndex];
		FrameCycles[NextIndex] = Cycles;
		WindowSumCycles += Cycles;

		NextIndex = (NextIndex + 1) % Duration;
		if (SampleCount < Duration)
			++SampleCount;
		
		if (Cycles > MaxFrameCycles)
			MaxFrameCycles = Cycles;
	}

	double GetRecentAverageMs() const
	{
		return SampleCount > 0
			? FPlatformTime::ToMilliseconds64(WindowSumCycles) / SampleCount
			: 0.0;
	}

	double GetMaxMs() const
	{
		return FPlatformTime::ToMilliseconds64(MaxFrameCycles);
	}
};

// UE는 이 집계를 외부 프로파일러(Insights)에 맡기지만 우리는 직접 모은다.
class FStatRegistry
{
public:
	static void BeginFrame()
	{
		std::swap(WriteStats, ReadStats);

		// 처음 측정된 항목 추가
		for (const auto& [Name, Data] : ReadStats)
		{
			if (!Histories.Find(Name))
			{
				Histories.Add(Name, FStatHistory{});
			}
		}

		// 기록 갱신
		for (auto& [Name, History] : Histories)
		{
			const FCycleStatData* Data = ReadStats.FindOrNull(Name);
			History.PushFrame(Data ? Data->TotalCycles : 0);
		}

		WriteStats.Reset();
	}

	static void AddCycles(TStatId StatId, uint64 Cycles)
	{
		FCycleStatData& Data = WriteStats[StatId.GetName()];
		Data.LastCycles = Cycles;
		Data.TotalCycles += Cycles;
		++Data.CallCount;
	}

	static void ResetHistory()
	{
		for (auto& [Name, History] : Histories)
		{
			History = FStatHistory{};
		}
	}

	static const FCycleStatData* Find(TStatId StatId) { return ReadStats.Find(StatId.GetName()); }
	static const TMap<const char*, FCycleStatData>& GetLastFrameStats() { return ReadStats; }
	static const TMap<const char*, FStatHistory>& GetHistories() { return Histories; }
	static void ResetAll() { return ReadStats.Reset(); }

private:
	inline static TMap<const char*, FCycleStatData> WriteStats;
	inline static TMap<const char*, FCycleStatData> ReadStats;
	inline static TMap<const char*, FStatHistory> Histories;
};

// 생성 시 시작 사이클을 기록하고, 스코프를 벗어날 때 경과 사이클을 FStatRegistry에 보고한다.
class FScopeCycleCounter
{
public:
	explicit FScopeCycleCounter(TStatId InStatId)
		: StartCycles(FPlatformTime::GetCycles64())
		, StatId(InStatId)
	{
	}

	~FScopeCycleCounter()
	{
		if (StatId.IsValidStat())
			FStatRegistry::AddCycles(StatId, FPlatformTime::GetCycles64() - StartCycles);
	}

	// 복사되면 소멸자가 두 번 돌아 같은 측정이 두 번 누적된다.
	FScopeCycleCounter(const FScopeCycleCounter&) = delete;
	FScopeCycleCounter& operator=(const FScopeCycleCounter&) = delete;

private:
	uint64 StartCycles;
	TStatId StatId;
};

// 이름 문자열을 inline 배열로 한 번만 만들어 모든 번역 단위가 같은 주소를 보게 한다.
#define DECLARE_CYCLE_STAT(CounterName, StatId) \
	inline constexpr char StatId##_Name[] = CounterName; \
	inline constexpr TStatId StatId{ StatId##_Name }

#define GET_STATID(StatId) (StatId)

#define STATS_JOIN_INNER(A, B) A##B
#define STATS_JOIN(A, B) STATS_JOIN_INNER(A, B)
#define SCOPE_CYCLE_COUNTER(StatId) \
	FScopeCycleCounter STATS_JOIN(ScopeCycleCounter_, __LINE__)(GET_STATID(StatId))