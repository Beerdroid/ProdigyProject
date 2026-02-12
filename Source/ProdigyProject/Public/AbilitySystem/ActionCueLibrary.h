#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionCueLibrary.generated.h"


UCLASS()
class PRODIGYPROJECT_API UActionCueLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// Hit/damage cue
	static void PlayHitCue(UObject* WorldContextObject, AActor* TargetActor, AActor* InstigatorActor, float Damage, const FVector& OptionalWorldLocation);

	// Combat start/end cues
	static void PlayEnterCombatCue(UObject* WorldContextObject, AActor* FocusActor);
	static void PlayExitCombatCue(UObject* WorldContextObject, AActor* FocusActor);

	// Blocked action cue (invalid target etc.)
	static void PlayInvalidTargetCue(UObject* WorldContextObject, AActor* FocusActor);
};
