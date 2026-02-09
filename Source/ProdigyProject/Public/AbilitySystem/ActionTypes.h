#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionTypes.generated.h"

UENUM(BlueprintType)
enum class EActionTargetingMode : uint8
{
	None UMETA(DisplayName="None"),
	Self UMETA(DisplayName="Self"),
	Unit UMETA(DisplayName="Unit"),
	Point UMETA(DisplayName="Point"),
};

UENUM(BlueprintType)
enum class EActionFailReason : uint8
{
	None,

	NoDefinition,

	// New: mode gates (combat vs exploration)
	BlockedByMode,

	// Tags gates
	BlockedByTags,
	MissingRequiredTags,

	// Execution gates
	OnCooldown,
	InsufficientAP,
	InvalidTarget,
};

USTRUCT(BlueprintType)
struct FActionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) TObjectPtr<AActor> Instigator = nullptr;
	UPROPERTY(BlueprintReadWrite) TObjectPtr<AActor> TargetActor = nullptr;
	UPROPERTY(BlueprintReadWrite) FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite) FGameplayTag OptionalSubTarget;
};

USTRUCT(BlueprintType)
struct FActionQueryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bCanExecute = false;
	UPROPERTY(BlueprintReadOnly) EActionFailReason FailReason = EActionFailReason::None;

	UPROPERTY(BlueprintReadOnly) int32 APCost = 0;

	// Cooldown preview (UI)
	UPROPERTY(BlueprintReadOnly) int32 CooldownTurns = 0;
	UPROPERTY(BlueprintReadOnly) float CooldownSeconds = 0.f;

	// Optional UI preview fields
	UPROPERTY(BlueprintReadOnly) bool bTargetValid = true;
};

USTRUCT()
struct FActionCooldownState
{
	GENERATED_BODY()

	// Combat (turn-based)
	UPROPERTY() int32 TurnsRemaining = 0;

	// Exploration (real-time) - absolute timestamp
	UPROPERTY() float CooldownEndTime = 0.f;

	// Helpers (QueryAction passes "Now")
	bool IsOnCooldown(float Now) const
	{
		return (TurnsRemaining > 0) || (CooldownEndTime > 0.f && Now < CooldownEndTime);
	}

	float GetSecondsRemaining(float Now) const
	{
		return (CooldownEndTime > 0.f) ? FMath::Max(0.f, CooldownEndTime - Now) : 0.f;
	}
};
