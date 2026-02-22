#pragma once

#include "CoreMinimal.h"
#include "Character/CombatantCharacterBase.h"
#include "EnemyCombatantCharacter.generated.h"

UCLASS()
class PRODIGYPROJECT_API AEnemyCombatantCharacter : public ACombatantCharacterBase
{
	GENERATED_BODY()
public:
	AEnemyCombatantCharacter();

	virtual void BeginPlay() override;

	// Aggro enabled?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Aggro")
	bool bAggressive = true;

	// Main aggro radius (also used to configure sight)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Aggro", meta=(ClampMin="0.0"))
	float AggroRadius = 900.f;

	// When aggro triggers, pull nearby enemies into the same combat
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Aggro", meta=(ClampMin="0.0"))
	float AllyJoinRadius = 1100.f;

	// NEW: Faction identity (used to decide who counts as "ally")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Faction")
	FGameplayTag FactionTag;
};