#include "EnginePCH.h"

// Source/Editor/main.cpp  — exe. 여기서만 구체 타입을 안다.
#include "Launch/EntryPoint.h"

#include "Editor/HitoriEd/Engine.h"
#include "Programs/ObjViewer/ObjViewerApp.h"
#include "Programs/Benchmark/BenchmarkApp.h"

UClass* GetEngineClass()
{
#ifdef OBJ_VIEWER
	return UObjViewerEngine::StaticClass();
#elif BENCHMARK
	return UBenchmarkEngine::StaticClass();
#else
	return UEditorEngine::StaticClass();
#endif
}
