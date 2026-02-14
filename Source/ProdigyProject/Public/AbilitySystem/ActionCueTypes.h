#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActionCueTypes.generated.h"

class UNiagaraSystem;
class USoundBase;

UENUM(BlueprintType)
enum class EActionCueLocation : uint8
{
	Instigator UMETA(DisplayName="Instigator"),
	Target     UMETA(DisplayName="Target"),
	Impact     UMETA(DisplayName="Impact (Context Location)"),
};

USTRUCT(BlueprintType)
struct FActionCueDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UNiagaraSystem> VFX = nullptr;

	// Where to play the cue (attach/emit at)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EActionCueLocation Location = EActionCueLocation::Target;

	// If set, attaches to this socket on the chosen actor
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AttachSocket = NAME_None;

	// If true, VFX will attach (if actor + component valid)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAttachVFX = true;

	// If true, Sound will attach (if actor + component valid)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAttachSound = false;

	// Simple spam guard (seconds). 0 = no guard.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CooldownSeconds = 0.0f;

	// For concurrency scoping: if true, cooldown is per-instigator; else global per cue tag
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCooldownPerInstigator = true;
};
