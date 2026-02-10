#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CombatCueSet.generated.h"

class UNiagaraSystem;
class USoundBase;

UENUM(BlueprintType)
enum class ECombatCueAnchor : uint8
{
	Instigator UMETA(DisplayName="Instigator"),
	Target     UMETA(DisplayName="Target"),
	Location   UMETA(DisplayName="Location"),
};

USTRUCT(BlueprintType)
struct FCombatCueDef
{
	GENERATED_BODY()

	// Niagara FX (optional)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cue")
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	// Sound (optional)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cue")
	TObjectPtr<USoundBase> Sound = nullptr;

	// Spawn behavior
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cue")
	bool bAttachToActor = false;

	// Used only when bAttachToActor == true
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cue", meta=(EditCondition="bAttachToActor"))
	FName AttachSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cue")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cue")
	FRotator RotationOffset = FRotator::ZeroRotator;

	// If false, subsystem uses a default anchor provided by caller/effect.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cue")
	bool bOverrideAnchor = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cue", meta=(EditCondition="bOverrideAnchor"))
	ECombatCueAnchor AnchorOverride = ECombatCueAnchor::Target;
};

UCLASS(BlueprintType)
class PRODIGYPROJECT_API UCombatCueSet : public UDataAsset
{
	GENERATED_BODY()

public:
	// Map CueTag -> Definition
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cue")
	TMap<FGameplayTag, FCombatCueDef> Cues;

	UFUNCTION(BlueprintCallable, Category="Cue")
	bool TryGetCueDef(FGameplayTag CueTag, FCombatCueDef& OutDef) const;
};
