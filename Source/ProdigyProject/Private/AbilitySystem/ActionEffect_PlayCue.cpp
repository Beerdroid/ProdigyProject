#include "AbilitySystem/ActionEffect_PlayCue.h"

#include "AbilitySystem/ActionCueSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogCueEffect, Log, All);

bool UActionEffect_PlayCue::Apply_Implementation(const FActionContext& Context) const
{
	if (!CueTag.IsValid())
	{
		UE_LOG(LogCueEffect, Error, TEXT("PlayCue effect failed: invalid CueTag"));
		return false;
	}

	UWorld* World = nullptr;

	// Prefer instigator world, else target
	if (IsValid(Context.Instigator)) World = Context.Instigator->GetWorld();
	if (!World && IsValid(Context.TargetActor)) World = Context.TargetActor->GetWorld();

	if (!World)
	{
		UE_LOG(LogCueEffect, Error, TEXT("PlayCue effect failed: no World (CueTag=%s)"), *CueTag.ToString());
		return false;
	}

	UActionCueSubsystem* Subsys = World->GetSubsystem<UActionCueSubsystem>();
	if (!Subsys)
	{
		UE_LOG(LogCueEffect, Error, TEXT("PlayCue effect failed: missing ActionCueSubsystem (CueTag=%s)"), *CueTag.ToString());
		return false;
	}

	// Bridge old context -> new cue context (no renames to FActionContext)
	FActionCueContext Ctx;
	Ctx.InstigatorActor = Context.Instigator;
	Ctx.TargetActor     = Context.TargetActor;

	// Prefer explicit TargetLocation if set; else fall back to TargetActor location; else zero.
	if (!Context.TargetLocation.IsNearlyZero())
	{
		Ctx.Location = Context.TargetLocation;
	}
	else if (IsValid(Context.TargetActor))
	{
		Ctx.Location = Context.TargetActor->GetActorLocation();
	}
	else
	{
		Ctx.Location = FVector::ZeroVector;
	}

	// If your FActionCueContext has OptionalSubTarget, keep it.
	Ctx.OptionalSubTarget = Context.OptionalSubTarget;

	Subsys->PlayCue(CueTag, Ctx);
	return true;
}
