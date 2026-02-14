#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionCueLibrary.generated.h"


UCLASS()
class PRODIGYPROJECT_API UActionCueLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Hit/damage cue
	UFUNCTION(BlueprintCallable, Category="Cues", meta=(WorldContext="WorldContextObject"))
	static void PlayHitCue(UObject* WorldContextObject, AActor* TargetActor, AActor* InstigatorActor, float Damage,
	                       const FVector& OptionalWorldLocation);

	// Combat start/end cues
	UFUNCTION(BlueprintCallable, Category="Cues", meta=(WorldContext="WorldContextObject"))
	static void PlayEnterCombatCue(UObject* WorldContextObject, AActor* FocusActor);

	UFUNCTION(BlueprintCallable, Category="Cues", meta=(WorldContext="WorldContextObject"))
	static void PlayExitCombatCue(UObject* WorldContextObject, AActor* FocusActor);

	// Blocked action cue (invalid target etc.)
	UFUNCTION(BlueprintCallable, Category="Cues", meta=(WorldContext="WorldContextObject"))
	static void PlayInvalidTargetCue(UObject* WorldContextObject, AActor* FocusActor);

	UFUNCTION(BlueprintCallable, Category="Cues", meta=(WorldContext="WorldContextObject"))
	static void PlayHitCue_Layered(
		UObject* WorldContextObject,
		AActor* TargetActor,
		AActor* InstigatorActor,
		float Damage,
		const FVector& OptionalWorldLocation,
		FGameplayTag WeaponTag,
		FGameplayTag SurfaceTage);
};
