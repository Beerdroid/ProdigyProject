#include "ActionCueLibrary.h"

#include "ActionCueSubsystem.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "ProdigyGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionCue, Log, All);

static UActionCueSubsystem* GetCueSubsystem(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogActionCue, Error, TEXT("GetCueSubsystem: WorldContextObject is NULL"));
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		UE_LOG(LogActionCue, Error, TEXT("GetCueSubsystem: GetWorld() is NULL for %s (Class=%s)"),
			*GetNameSafe(WorldContextObject),
			*GetNameSafe(WorldContextObject->GetClass()));
		return nullptr;
	}

	UE_LOG(LogActionCue, Verbose, TEXT("GetCueSubsystem: World=%s NetMode=%d"),
		*GetNameSafe(World),
		(int32)World->GetNetMode());

	UActionCueSubsystem* Sub = World->GetSubsystem<UActionCueSubsystem>();
	UE_LOG(LogActionCue, Warning, TEXT("GetCueSubsystem: Subsystem=%s"), *GetNameSafe(Sub));
	return Sub;
}

void UActionCueLibrary::PlayHitCue(
	UObject* WorldContextObject,
	AActor* TargetActor,
	AActor* InstigatorActor,
	float Damage,
	const FVector& OptionalWorldLocation)
{
	UE_LOG(LogActionCue, Warning, TEXT("UActionCueLibrary PlayHitCue start"));
	UActionCueSubsystem* Cues = GetCueSubsystem(WorldContextObject);
	if (!Cues) return;

	UE_LOG(LogActionCue, Warning, TEXT("UActionCueLibrary Cues found"));

	if (!IsValid(TargetActor)) return;
	if (Damage <= 0.f) return;

	UE_LOG(LogActionCue, Warning, TEXT("UActionCueLibrary before FActionCueContext"));

	FActionCueContext Ctx;
	Ctx.InstigatorActor = InstigatorActor;
	Ctx.TargetActor = TargetActor;
	Ctx.Location = OptionalWorldLocation.IsNearlyZero()
		? TargetActor->GetActorLocation()
		: OptionalWorldLocation;

	UE_LOG(LogActionCue, Warning, TEXT("[HitCue] Inst=%s Target=%s Damage=%.1f"),
	*GetNameSafe(InstigatorActor),
	*GetNameSafe(TargetActor),
	Damage);

	// If you need damage for selection rules later, add it to Ctx (only if it already exists).
	Cues->PlayCue(ActionCueTags::Cue_Action_Hit, Ctx);
	
	UE_LOG(LogActionCue, Warning, TEXT("UActionCueLibrary PlayHitCue AFTER PlayCue"));
}

void UActionCueLibrary::PlayEnterCombatCue(UObject* WorldContextObject, AActor* FocusActor)
{
	UActionCueSubsystem* Cues = GetCueSubsystem(WorldContextObject);
	if (!Cues) return;

	FActionCueContext Ctx;
	Ctx.InstigatorActor = FocusActor;
	Ctx.TargetActor = nullptr;
	Ctx.Location = FVector::ZeroVector;

	Cues->PlayCue(ActionCueTags::Cue_Combat_Enter, Ctx);
}

void UActionCueLibrary::PlayExitCombatCue(UObject* WorldContextObject, AActor* FocusActor)
{
	UActionCueSubsystem* Cues = GetCueSubsystem(WorldContextObject);
	if (!Cues) return;

	FActionCueContext Ctx;
	Ctx.InstigatorActor = FocusActor;
	Ctx.TargetActor = nullptr;
	Ctx.Location = FVector::ZeroVector;

	Cues->PlayCue(ActionCueTags::Cue_Combat_Exit, Ctx);
}

void UActionCueLibrary::PlayInvalidTargetCue(UObject* WorldContextObject, AActor* FocusActor)
{
	UActionCueSubsystem* Cues = GetCueSubsystem(WorldContextObject);
	if (!Cues) return;

	FActionCueContext Ctx;
	Ctx.InstigatorActor = FocusActor;
	Ctx.TargetActor = FocusActor;
	Ctx.Location = FVector::ZeroVector;

	Cues->PlayCue(ActionCueTags::Cue_Target_Invalid, Ctx);
}