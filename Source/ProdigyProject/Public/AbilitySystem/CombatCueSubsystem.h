#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "ActionTypes.h"
#include "CombatCueSet.h"
#include "CombatCueSubsystem.generated.h"

UCLASS()
class PRODIGYPROJECT_API UCombatCueSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Set this in World Settings via a BP that sets it on BeginPlay, or from GameMode/GameState.
	// No magic lookup.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cue")
	TObjectPtr<UCombatCueSet> CueSet = nullptr;

	UFUNCTION(BlueprintCallable, Category="Cue")
	bool ExecuteCue(FGameplayTag CueTag, const FActionContext& Context, ECombatCueAnchor DefaultAnchor);

private:
	bool ResolveAnchorTransform(const FActionContext& Context, ECombatCueAnchor Anchor, FVector& OutLoc, FRotator& OutRot) const;
	AActor* ResolveAnchorActor(const FActionContext& Context, ECombatCueAnchor Anchor) const;
};
