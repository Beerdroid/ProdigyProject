#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldCombatEvents.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOnWorldDamageEvent,
	AActor*, TargetActor,
	AActor*, InstigatorActor,
	float, AppliedDamage,
	float, OldHP,
	float, NewHP
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOnWorldHealEvent,
	AActor*, TargetActor,
	AActor*, InstigatorActor,
	float, AppliedHeal,
	float, OldHP,
	float, NewHP
);

// Generic request to start combat (aggro / external systems)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnCombatStartRequested,
	AActor*, AggroActor,
	AActor*, TargetActor,
	AActor*, FirstToAct
);

// NEW: emitted by action execution when an effect was executed against a target.
// No "success" semantics required; this is a notification hook.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnWorldEffectApplied,
	AActor*, TargetActor,
	AActor*, InstigatorActor,
	FGameplayTag, ActionTag
);

UCLASS()
class PRODIGYPROJECT_API UWorldCombatEvents : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FOnWorldDamageEvent OnWorldDamageEvent;

	UPROPERTY(BlueprintAssignable)
	FOnWorldHealEvent OnWorldHealEvent;

	UPROPERTY(BlueprintAssignable)
	FOnCombatStartRequested OnCombatStartRequested;

	// NEW
	UPROPERTY(BlueprintAssignable)
	FOnWorldEffectApplied OnWorldEffectApplied;
};