#include "AbilitySystem/CombatCueSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatCues, Log, All);

bool UCombatCueSubsystem::ExecuteCue(FGameplayTag CueTag, const FActionContext& Context, ECombatCueAnchor DefaultAnchor)
{
	if (!CueTag.IsValid())
	{
		UE_LOG(LogCombatCues, Error, TEXT("ExecuteCue failed: invalid CueTag"));
		return false;
	}

	if (!CueSet)
	{
		UE_LOG(LogCombatCues, Error, TEXT("ExecuteCue failed: CueSet is null (CueTag=%s)"), *CueTag.ToString());
		return false;
	}

	FCombatCueDef Def;
	if (!CueSet->TryGetCueDef(CueTag, Def))
	{
		UE_LOG(LogCombatCues, Error, TEXT("ExecuteCue failed: CueTag not found in CueSet: %s"), *CueTag.ToString());
		return false;
	}

	const ECombatCueAnchor Anchor = Def.bOverrideAnchor ? Def.AnchorOverride : DefaultAnchor;

	// Determine spawn transform
	FVector Loc = FVector::ZeroVector;
	FRotator Rot = FRotator::ZeroRotator;
	if (!ResolveAnchorTransform(Context, Anchor, Loc, Rot))
	{
		UE_LOG(LogCombatCues, Error, TEXT("ExecuteCue failed: could not resolve anchor transform (CueTag=%s Anchor=%d)"),
			*CueTag.ToString(), (int32)Anchor);
		return false;
	}

	Loc += Def.LocationOffset;
	Rot = (Rot + Def.RotationOffset);

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogCombatCues, Error, TEXT("ExecuteCue failed: no World (CueTag=%s)"), *CueTag.ToString());
		return false;
	}

	AActor* AnchorActor = Def.bAttachToActor ? ResolveAnchorActor(Context, Anchor) : nullptr;

	// Niagara
	if (Def.NiagaraSystem)
	{
		if (Def.bAttachToActor)
		{
			if (!IsValid(AnchorActor))
			{
				UE_LOG(LogCombatCues, Error, TEXT("ExecuteCue failed: attach requested but anchor actor invalid (CueTag=%s)"),
					*CueTag.ToString());
				return false;
			}

			UNiagaraFunctionLibrary::SpawnSystemAttached(
				Def.NiagaraSystem,
				AnchorActor->GetRootComponent(),
				Def.AttachSocket,
				Def.LocationOffset,
				Def.RotationOffset,
				EAttachLocation::KeepRelativeOffset,
				true // bAutoDestroy
			);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Def.NiagaraSystem, Loc, Rot);
		}
	}

	// Sound
	if (Def.Sound)
	{
		if (Def.bAttachToActor)
		{
			if (!IsValid(AnchorActor))
			{
				UE_LOG(LogCombatCues, Error, TEXT("ExecuteCue failed: attach requested but anchor actor invalid for sound (CueTag=%s)"),
					*CueTag.ToString());
				return false;
			}

			UGameplayStatics::SpawnSoundAttached(
				Def.Sound,
				AnchorActor->GetRootComponent(),
				Def.AttachSocket,
				Def.LocationOffset,
				Def.RotationOffset,
				EAttachLocation::KeepRelativeOffset,
				true // bStopWhenAttachedToDestroyed
			);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(
				World,
				Def.Sound,
				Loc
			);
		}
	}

	return true;
}

AActor* UCombatCueSubsystem::ResolveAnchorActor(const FActionContext& Context, ECombatCueAnchor Anchor) const
{
	switch (Anchor)
	{
	case ECombatCueAnchor::Instigator:
		return Context.Instigator;
	case ECombatCueAnchor::Target:
		return Context.TargetActor;
	case ECombatCueAnchor::Location:
	default:
		return nullptr;
	}
}

bool UCombatCueSubsystem::ResolveAnchorTransform(const FActionContext& Context, ECombatCueAnchor Anchor, FVector& OutLoc, FRotator& OutRot) const
{
	switch (Anchor)
	{
	case ECombatCueAnchor::Instigator:
		if (!IsValid(Context.Instigator)) return false;
		OutLoc = Context.Instigator->GetActorLocation();
		OutRot = Context.Instigator->GetActorRotation();
		return true;

	case ECombatCueAnchor::Target:
		if (!IsValid(Context.TargetActor)) return false;
		OutLoc = Context.TargetActor->GetActorLocation();
		OutRot = Context.TargetActor->GetActorRotation();
		return true;

	case ECombatCueAnchor::Location:
		// Explicit: for location cues, Context.TargetLocation MUST be provided
		if (Context.TargetLocation.IsNearlyZero()) return false;
		OutLoc = Context.TargetLocation;
		OutRot = FRotator::ZeroRotator;
		return true;

	default:
		return false;
	}
}
