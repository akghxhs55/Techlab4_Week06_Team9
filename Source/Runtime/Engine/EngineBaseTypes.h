#pragma once

#include "Core/Types.h"

class AActor;

struct FTickFunction
{
public:
	FTickFunction() = default;
	virtual ~FTickFunction() = default;

	virtual void ExecuteTick(float _deltaTime) = 0;

	uint8 bTickEvenWhenPaused : 1;
	uint8 bCanEverTick : 1;
	uint8 bStartWithTickEnabled : 1;

	enum class ETickState : uint8
	{
		// This tick will not execute in the next/current frame.
		Disabled,
		// This tick will execute normally.
		Enabled,
		// This tick has a tick interval that is counting down before the next execution.
		CoolingDown
	};

	/** Internal tick state, set by tick manager every frame. */
	ETickState TickState : 2;
public:
	float TickInterval;

};

struct FActorTickFunction : public FTickFunction
{
	AActor* Target;

	virtual void ExecuteTick(float DeltaTime) override;
};