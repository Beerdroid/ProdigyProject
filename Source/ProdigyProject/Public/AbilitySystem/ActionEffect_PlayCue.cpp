#include "AbilitySystem/ActionEffect_PlayCue.h"

#include "AbilitySystem/CombatCueSubsystem.h"

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

	UCombatCueSubsystem* Subsys = World->GetSubsystem<UCombatCueSubsystem>();
	if (!Subsys)
	{
		UE_LOG(LogCueEffect, Error, TEXT("PlayCue effect failed: missing CombatCueSubsystem (CueTag=%s)"), *CueTag.ToString());
		return false;
	}

	return Subsys->ExecuteCue(CueTag, Context, DefaultAnchor);
}
