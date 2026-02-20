#pragma once

#include "CoreMinimal.h"
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

UCLASS()
class PRODIGYPROJECT_API UWorldCombatEvents : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FOnWorldDamageEvent OnWorldDamageEvent;

	UPROPERTY(BlueprintAssignable)
	FOnWorldHealEvent OnWorldHealEvent;
};
