#include "AbilitySystem/ActionCueLibrary.h"

#include "AbilitySystem/ActionCueSubsystem.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystem/ProdigyGameplayTags.h"

static UActionCueSubsystem* GetCueSubsystem(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogActionCue, Error, TEXT("GetCueSubsystem: WorldContextObject is NULL"));
		return nullptr;
	}

	if (!GEngine)
	{
		UE_LOG(LogActionCue, Error, TEXT("GetCueSubsystem: GEngine is NULL"));
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogActionCue, Error, TEXT("GetCueSubsystem: World is NULL for %s (Class=%s)"),
			*GetNameSafe(WorldContextObject),
			*GetNameSafe(WorldContextObject->GetClass()));
		return nullptr;
	}

	UE_LOG(LogActionCue, Verbose, TEXT("GetCueSubsystem: World=%s NetMode=%d"),
		*GetNameSafe(World),
		(int32)World->GetNetMode());

	UActionCueSubsystem* Sub = World->GetSubsystem<UActionCueSubsystem>();

	// This was Warning before; make it Verbose to avoid constant spam
	UE_LOG(LogActionCue, Verbose, TEXT("GetCueSubsystem: Subsystem=%s"), *GetNameSafe(Sub));

	return Sub;
}

void UActionCueLibrary::PlayHitCue(
	UObject* WorldContextObject,
	AActor* TargetActor,
	AActor* InstigatorActor,
	float Damage,
	const FVector& OptionalWorldLocation)
{
	// Validate first (avoid subsystem lookup + logs on no-op)
	if (!IsValid(TargetActor)) return;
	if (Damage <= 0.f) return;

	UE_LOG(LogActionCue, Verbose, TEXT("UActionCueLibrary PlayHitCue start"));

	UActionCueSubsystem* Cues = GetCueSubsystem(WorldContextObject);
	if (!Cues) return;

	FActionCueContext Ctx;
	Ctx.InstigatorActor = InstigatorActor;
	Ctx.TargetActor = TargetActor;
	Ctx.Location = OptionalWorldLocation.IsNearlyZero()
		? TargetActor->GetActorLocation()
		: OptionalWorldLocation;

	UE_LOG(LogActionCue, Verbose, TEXT("[HitCue] Inst=%s Target=%s Damage=%.1f"),
		*GetNameSafe(InstigatorActor),
		*GetNameSafe(TargetActor),
		Damage);

	Cues->PlayCue(ActionCueTags::Cue_Action_Hit, Ctx);
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
	Ctx.TargetActor = nullptr;
	Ctx.Location = IsValid(FocusActor) ? FocusActor->GetActorLocation() : FVector::ZeroVector;

	Cues->PlayCue(ActionCueTags::Cue_Target_Invalid, Ctx);
}

void UActionCueLibrary::PlayHitCue_Layered(
	UObject* WorldContextObject,
	AActor* TargetActor,
	AActor* InstigatorActor,
	float Damage,
	const FVector& OptionalWorldLocation,
	FGameplayTag WeaponTag,
	FGameplayTag SurfaceTag)
{
	UActionCueSubsystem* Cues = GetCueSubsystem(WorldContextObject);
	if (!Cues) return;

	if (!IsValid(TargetActor)) return;
	if (Damage <= 0.f) return;

	FActionCueContext Ctx;
	Ctx.InstigatorActor = InstigatorActor;
	Ctx.TargetActor = TargetActor;
	Ctx.Location = OptionalWorldLocation.IsNearlyZero()
		? TargetActor->GetActorLocation()
		: OptionalWorldLocation;

	Ctx.WeaponTag = WeaponTag;
	Ctx.SurfaceTag = SurfaceTag;

	Cues->PlayCue(ActionCueTags::Cue_Action_Hit, Ctx);
}