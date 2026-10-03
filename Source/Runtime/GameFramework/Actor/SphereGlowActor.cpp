#include "EnginePCH.h"
#include "SphereGlowActor.h"

ASphereGlowActor::ASphereGlowActor()
{
    SphereGlowComponent = CreateDefaultSubobject<USphereGlowComponent>("USphereGlowComponent");
    SetRootComponent(SphereGlowComponent);
}

USphereGlowComponent* ASphereGlowActor::GetSphereGlowComponent() const
{
    return static_cast<USphereGlowComponent*>(RootComponent);
}