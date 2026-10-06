#include "EnginePCH.h"

#include "Launch/EntryPoint.h"

#include "Editor/HitoriEd/EditorEngine.h"
#include "Programs/ObjViewer/ObjViewerApp.h"

UClass* GetEngineClass()
{
#ifdef OBJ_VIEWER
	return UObjViewerEngine::StaticClass();
#else
	return UEditorEngine::StaticClass();
#endif
}
