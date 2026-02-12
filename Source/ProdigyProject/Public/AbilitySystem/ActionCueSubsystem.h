#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "ActionCueTypes.h"
#include "ActionCueSubsystem.generated.h"

class UActionCueSet;

DECLARE_LOG_CATEGORY_EXTERN(LogActionCue, Log, All);

USTRUCT(BlueprintType)
struct FActionCueContext
{
    GENERATED_BODY()

    // Who caused the cue (often attacker / ability owner)
    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<AActor> InstigatorActor = nullptr;

    // Who the cue is about (often victim / target)
    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<AActor> TargetActor = nullptr;

    // Optional point in space (hit point, cursor point, etc.)
    // If you already have TargetLocation semantics, keep it consistent:
    UPROPERTY(BlueprintReadWrite)
    FVector Location = FVector::ZeroVector;

    // Optional tag for variants (headshot, weakspot, etc.)
    UPROPERTY(BlueprintReadWrite)
    FGameplayTag OptionalSubTarget;
};

UCLASS()
class PRODIGYPROJECT_API UActionCueSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    // Main API: call this from gameplay code
    UFUNCTION(BlueprintCallable, Category="Cues")
    void PlayCue(const FGameplayTag& CueTag, const FActionCueContext& Ctx);

    // Resolver uses layered providers (Step D)
    bool ResolveCue(const FGameplayTag& CueTag, const FActionCueContext& Ctx, FActionCueDef& OutDef) const;

    // Optional: if you have action-specific cue sets, call this before PlayCue:
    void PushActionOverrideCueSet(UActionCueSet* InOverride);
    void PopActionOverrideCueSet();

private:
    // Stack so ExecuteAction can set an override for “this action”
    UPROPERTY(Transient)
    TArray<TObjectPtr<UActionCueSet>> ActionOverrideStack;

    // Cooldown bookkeeping
    mutable TMap<uint64, double> LastPlayedTimeByKey;

    uint64 MakeCooldownKey(const FGameplayTag& CueTag, const FActionCueDef& Def, const FActionCueContext& Ctx) const;

    bool PassesCooldown(const FGameplayTag& CueTag, const FActionCueDef& Def, const FActionCueContext& Ctx) const;
    void MarkPlayed(const FGameplayTag& CueTag, const FActionCueDef& Def, const FActionCueContext& Ctx) const;

    // Spawning
    AActor* ResolveLocationActor(const FActionCueDef& Def, const FActionCueContext& Ctx) const;
    FVector ResolveLocation(const FActionCueDef& Def, const FActionCueContext& Ctx) const;
    USceneComponent* ResolveAttachComponent(AActor* A) const;

    // Filtering dead targets etc. (you’ll wire IsDeadByAttributes below)
    bool IsAttackableTarget(const FActionCueContext& Ctx) const;
};
