#include "AbilitySystem/ActionCueSubsystem.h"

#include "AbilitySystem/ActionCueSettings.h"
#include "AbilitySystem/ActionCueSet.h"
#include "AbilitySystem/ActionCueProviderComponent.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystem/ProdigyAbilityUtils.h"
#include "Sound/SoundBase.h"
#include "Components/SceneComponent.h"

DEFINE_LOG_CATEGORY(LogActionCue);

static FRotator ResolveCueRotation(const FActionCueDef& Def, AActor* TargetActor, AActor* InstigatorActor, const FVector& Loc)
{
    switch (Def.Location)
    {
    case EActionCueLocation::Instigator:
        return IsValid(InstigatorActor) ? InstigatorActor->GetActorRotation() : FRotator::ZeroRotator;

    case EActionCueLocation::Target:
        return IsValid(TargetActor) ? TargetActor->GetActorRotation() : FRotator::ZeroRotator;

    case EActionCueLocation::Impact:
    default:
        break;
    }

    if (IsValid(InstigatorActor))
    {
        const FVector From = InstigatorActor->GetActorLocation();
        const FVector Dir = (Loc - From);
        if (!Dir.IsNearlyZero())
        {
            return Dir.Rotation();
        }
    }

    return FRotator::ZeroRotator;
}

static UActionCueSet* LoadCueSetSoft(const TSoftObjectPtr<UActionCueSet>& SoftPtr)
{
    return SoftPtr.IsNull() ? nullptr : SoftPtr.LoadSynchronous();
}

void UActionCueSubsystem::PushActionOverrideCueSet(UActionCueSet* InOverride)
{
    if (InOverride)
    {
        ActionOverrideStack.Add(InOverride);
    }
}

void UActionCueSubsystem::PopActionOverrideCueSet()
{
    if (ActionOverrideStack.Num() > 0)
    {
        ActionOverrideStack.Pop(false);
    }
}

void UActionCueSubsystem::PlayCue(const FGameplayTag& CueTag, const FActionCueContext& Ctx)
{
	UE_LOG(LogActionCue, Warning, TEXT("[Cue] PlayCue start"));

	if (!CueTag.IsValid())
	{
		UE_LOG(LogActionCue, Warning, TEXT("[Cue] PlayCue called with INVALID tag"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogActionCue, Error, TEXT("[Cue] PlayCue aborted: GetWorld() is NULL (Tag=%s)"), *CueTag.ToString());
		return;
	}

	UE_LOG(LogActionCue, Warning,
		TEXT("[Cue] PlayCue START Tag=%s Inst=%s Target=%s CtxLoc=(%.1f %.1f %.1f)"),
		*CueTag.ToString(),
		*GetNameSafe(Ctx.InstigatorActor),
		*GetNameSafe(Ctx.TargetActor),
		Ctx.Location.X, Ctx.Location.Y, Ctx.Location.Z);

	if (IsValid(Ctx.TargetActor) && !IsAttackableTarget(Ctx))
	{
		UE_LOG(LogActionCue, Warning,
			TEXT("[Cue] BLOCKED by IsAttackableTarget Tag=%s Target=%s"),
			*CueTag.ToString(),
			*GetNameSafe(Ctx.TargetActor));
		return;
	}

	FActionCueDef Def;
	if (!ResolveCue(CueTag, Ctx, Def))
	{
		UE_LOG(LogActionCue, Warning,
			TEXT("[Cue] ResolveCue MISS Tag=%s (no cue def found)"),
			*CueTag.ToString());
		return;
	}

	if (!PassesCooldown(CueTag, Def, Ctx))
	{
		UE_LOG(LogActionCue, Warning,
			TEXT("[Cue] BLOCKED by Cooldown Tag=%s"),
			*CueTag.ToString());
		return;
	}

	AActor* LocActor = ResolveLocationActor(Def, Ctx);
	const FVector Loc = ResolveLocation(Def, Ctx);
	const FRotator Rot = ResolveCueRotation(Def, Ctx.TargetActor, Ctx.InstigatorActor, Loc);

	USceneComponent* AttachComp = LocActor ? ResolveAttachComponent(LocActor) : nullptr;

	UE_LOG(LogActionCue, Warning,
		TEXT("[Cue] Resolved Tag=%s DefLoc=%d LocActor=%s AttachComp=%s Socket=%s Loc=(%.1f %.1f %.1f) Rot=(%.1f %.1f %.1f)"),
		*CueTag.ToString(),
		(int32)Def.Location,
		*GetNameSafe(LocActor),
		*GetNameSafe(AttachComp),
		*Def.AttachSocket.ToString(),
		Loc.X, Loc.Y, Loc.Z,
		Rot.Pitch, Rot.Yaw, Rot.Roll);

	// --- VFX ---
	if (IsValid(Def.VFX))
	{
		const bool bCanAttach = Def.bAttachVFX && IsValid(AttachComp) && (Def.Location != EActionCueLocation::Impact);

		UE_LOG(LogActionCue, Warning,
			TEXT("[Cue] VFX Tag=%s VFX=%s Attach=%d"),
			*CueTag.ToString(),
			*GetNameSafe(Def.VFX),
			bCanAttach ? 1 : 0);

		if (bCanAttach)
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				Def.VFX,
				AttachComp,
				Def.AttachSocket,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				true
			);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				Def.VFX,
				Loc,
				Rot
			);
		}
	}
	else
	{
		UE_LOG(LogActionCue, Warning, TEXT("[Cue] No VFX for Tag=%s"), *CueTag.ToString());
	}

	// --- Sound ---
	auto SpawnOneSound = [&](USoundBase* Snd)
	{
		if (!IsValid(Snd)) return;

		const bool bCanAttach = Def.bAttachSound && IsValid(AttachComp) && (Def.Location != EActionCueLocation::Impact);

		if (bCanAttach)
		{
			UGameplayStatics::SpawnSoundAttached(
				Snd,
				AttachComp,
				Def.AttachSocket,
				FVector::ZeroVector,
				EAttachLocation::SnapToTarget,
				true
			);
		}
		else
		{
			UGameplayStatics::SpawnSoundAtLocation(GetWorld(), Snd, Loc);
		}
	};

	USoundBase* WeaponMS = nullptr;
	USoundBase* SurfaceMS = nullptr;

	if (UActionCueSet* Set = ResolveCueSet(CueTag, Ctx))
	{
		Set->ResolveMetaSoundsForContext(CueTag, Ctx.WeaponTag, Ctx.SurfaceTag, WeaponMS, SurfaceMS);
	}

	// Optional legacy single sound
	if (IsValid(Def.Sound))
	{
		SpawnOneSound(Def.Sound);
	}

	// Layered metasounds (weapon + surface)
	if (IsValid(WeaponMS))
	{
		SpawnOneSound(WeaponMS);
	}
	if (IsValid(SurfaceMS))
	{
		SpawnOneSound(SurfaceMS);
	}

	if (!IsValid(Def.Sound) && !IsValid(WeaponMS) && !IsValid(SurfaceMS))
	{
		UE_LOG(LogActionCue, Warning, TEXT("[Cue] No Sound for Tag=%s"), *CueTag.ToString());
	}

	MarkPlayed(CueTag, Def, Ctx);

	UE_LOG(LogActionCue, Warning, TEXT("[Cue] PlayCue END Tag=%s"), *CueTag.ToString());
}


bool UActionCueSubsystem::ResolveCue(const FGameplayTag& CueTag, const FActionCueContext& Ctx, FActionCueDef& OutDef) const
{
	if (UActionCueSet* Set = ResolveCueSet(CueTag, Ctx))
	{
		return Set->TryGetCue(CueTag, OutDef);
	}
	return false;
}

uint64 UActionCueSubsystem::MakeCooldownKey(const FGameplayTag& CueTag, const FActionCueDef& Def, const FActionCueContext& Ctx) const
{
	const uint32 TagHash = GetTypeHash(CueTag);
	uint64 Key = (uint64)TagHash;

	if (Def.bCooldownPerInstigator && IsValid(Ctx.InstigatorActor))
	{
		// Use UObject identity rather than raw pointer bits
		const uint32 InstId = (uint32)Ctx.InstigatorActor->GetUniqueID();

		// 64-bit mix (same structure you used before, but with stable id)
		const uint64 V = (uint64)InstId;
		Key ^= (V + 0x9e3779b97f4a7c15ULL + (Key << 6) + (Key >> 2));
	}

	return Key;
}

bool UActionCueSubsystem::PassesCooldown(const FGameplayTag& CueTag, const FActionCueDef& Def, const FActionCueContext& Ctx) const
{
	if (Def.CooldownSeconds <= 0.f) return true;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogActionCue, Error, TEXT("[Cue] PassesCooldown: GetWorld() is NULL (Tag=%s)"), *CueTag.ToString());
		return false;
	}

	const double Now = World->GetTimeSeconds();
	const uint64 Key = MakeCooldownKey(CueTag, Def, Ctx);

	if (const double* Last = LastPlayedTimeByKey.Find(Key))
	{
		return (Now - *Last) >= (double)Def.CooldownSeconds;
	}
	return true;
}

void UActionCueSubsystem::MarkPlayed(const FGameplayTag& CueTag, const FActionCueDef& Def, const FActionCueContext& Ctx) const
{
	if (Def.CooldownSeconds <= 0.f) return;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogActionCue, Error, TEXT("[Cue] MarkPlayed: GetWorld() is NULL (Tag=%s)"), *CueTag.ToString());
		return;
	}

	const uint64 Key = MakeCooldownKey(CueTag, Def, Ctx);
	LastPlayedTimeByKey.FindOrAdd(Key) = World->GetTimeSeconds();
}

AActor* UActionCueSubsystem::ResolveLocationActor(const FActionCueDef& Def, const FActionCueContext& Ctx) const
{
    switch (Def.Location)
    {
        case EActionCueLocation::Instigator: return Ctx.InstigatorActor;
        case EActionCueLocation::Target:     return Ctx.TargetActor;
        case EActionCueLocation::Impact:     return nullptr;
        default:                             return Ctx.TargetActor;
    }
}

FVector UActionCueSubsystem::ResolveLocation(const FActionCueDef& Def, const FActionCueContext& Ctx) const
{
    if (Def.Location == EActionCueLocation::Impact)
    {
        return Ctx.Location;
    }

    if (AActor* A = ResolveLocationActor(Def, Ctx))
    {
        return A->GetActorLocation();
    }

    return Ctx.Location;
}

USceneComponent* UActionCueSubsystem::ResolveAttachComponent(AActor* A) const
{
    if (!IsValid(A)) return nullptr;

    // Prefer root component if it exists
    if (USceneComponent* Root = A->GetRootComponent())
    {
        return Root;
    }

    return nullptr;
}

bool UActionCueSubsystem::IsAttackableTarget(const FActionCueContext& Ctx) const
{
	if (!IsValid(Ctx.TargetActor)) return true;

	if (Ctx.TargetActor->IsActorBeingDestroyed()) return false;

	// Align with action/combat validity rules
	if (ProdigyAbilityUtils::IsDeadByAttributes(Ctx.TargetActor)) return false;

	return true;
}

UActionCueSet* UActionCueSubsystem::ResolveCueSet(const FGameplayTag& CueTag, const FActionCueContext& Ctx) const
{
	auto SetHasCue = [&](UActionCueSet* Set) -> bool
	{
		if (!IsValid(Set)) return false;

		// Cheap check without copying OutDef:
		return Set->Cues.Contains(CueTag);
		// If you prefer using API:
		// FActionCueDef Dummy;
		// return Set->TryGetCue(CueTag, Dummy);
	};

	// 1) Action override stack (top-most that has the cue)
	for (int32 i = ActionOverrideStack.Num() - 1; i >= 0; --i)
	{
		if (UActionCueSet* Set = ActionOverrideStack[i])
		{
			if (SetHasCue(Set)) return Set;
		}
	}

	// 2) Target provider (only if it has the cue)
	if (IsValid(Ctx.TargetActor))
	{
		if (const UActionCueProviderComponent* Prov = Ctx.TargetActor->FindComponentByClass<UActionCueProviderComponent>())
		{
			if (SetHasCue(Prov->CueSet)) return Prov->CueSet;
			
		}
	}

	// 3) Instigator provider (only if it has the cue)
	if (IsValid(Ctx.InstigatorActor))
	{
		if (const UActionCueProviderComponent* Prov = Ctx.InstigatorActor->FindComponentByClass<UActionCueProviderComponent>())
		{
			if (SetHasCue(Prov->CueSet)) return Prov->CueSet;
		}
	}

	// 4) Global defaults (only if it has the cue)
	const UActionCueSettings* Settings = GetDefault<UActionCueSettings>();
	if (Settings)
	{
		if (UActionCueSet* GlobalSet = LoadCueSetSoft(Settings->GlobalCueSet))
		{
			if (SetHasCue(GlobalSet)) return GlobalSet;
		}
	}

	return nullptr;
}
