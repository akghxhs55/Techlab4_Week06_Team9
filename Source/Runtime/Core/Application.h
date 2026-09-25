#pragma once
#include <Windows.h>

class FApplication
{
public:
    FApplication();
    virtual ~FApplication() = default;

    virtual bool Init(HINSTANCE hInstance);
    virtual void Run();
    virtual void Shutdown();
};