#include "ActionCueSubsystem.h"

#include "ActionCueSettings.h"
#include "ActionCueSet.h"
#include "ActionCueProviderComponent.h"

#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "Components/SceneComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionCue, Log, All);

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

	UE_LOG(LogActionCue, Warning,
		TEXT("[Cue] PlayCue START Tag=%s Inst=%s Target=%s CtxLoc=(%.1f %.1f %.1f)"),
		*CueTag.ToString(),
		*GetNameSafe(Ctx.InstigatorActor),
		*GetNameSafe(Ctx.TargetActor),
		Ctx.Location.X, Ctx.Location.Y, Ctx.Location.Z);

	// Optional filter: only block when the cue actually cares about a target.
	// If you want EnterCombat/ExitCombat to always play, don't block when TargetActor is null.
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

	// Resolve where the cue should be played
	AActor* LocActor = ResolveLocationActor(Def, Ctx);
	const FVector Loc = ResolveLocation(Def, Ctx);
	const FRotator Rot = ResolveCueRotation(
		Def,
		Ctx.TargetActor,
		Ctx.InstigatorActor,
		Loc);

	// Resolve attachment target (the actor we actually attach to)
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
				GetWorld(),
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
	if (IsValid(Def.Sound))
	{
		const bool bCanAttach = Def.bAttachSound && IsValid(AttachComp) && (Def.Location != EActionCueLocation::Impact);

		UE_LOG(LogActionCue, Warning,
			TEXT("[Cue] SFX Tag=%s Sound=%s Attach=%d"),
			*CueTag.ToString(),
			*GetNameSafe(Def.Sound),
			bCanAttach ? 1 : 0);

		if (bCanAttach)
		{
			UGameplayStatics::SpawnSoundAttached(
				Def.Sound,
				AttachComp,
				Def.AttachSocket,
				FVector::ZeroVector,
				EAttachLocation::SnapToTarget,
				true
			);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(
				GetWorld(),
				Def.Sound,
				Loc
			);
		}
	}
	else
	{
		UE_LOG(LogActionCue, Warning, TEXT("[Cue] No Sound for Tag=%s"), *CueTag.ToString());
	}

	MarkPlayed(CueTag, Def, Ctx);
	

	UE_LOG(LogActionCue, Warning, TEXT("[Cue] PlayCue END Tag=%s"), *CueTag.ToString());
}

bool UActionCueSubsystem::ResolveCue(const FGameplayTag& CueTag, const FActionCueContext& Ctx, FActionCueDef& OutDef) const
{
    // 1) Action override (top of stack)
    for (int32 i = ActionOverrideStack.Num() - 1; i >= 0; --i)
    {
        if (UActionCueSet* Set = ActionOverrideStack[i])
        {
            if (Set->TryGetCue(CueTag, OutDef))
            {
                return true;
            }
        }
    }

    // 2) Target provider
    if (IsValid(Ctx.TargetActor))
    {
        if (const UActionCueProviderComponent* Prov = Ctx.TargetActor->FindComponentByClass<UActionCueProviderComponent>())
        {
            if (Prov->CueSet && Prov->CueSet->TryGetCue(CueTag, OutDef))
            {
                return true;
            }
        }
    }

    // 3) Instigator provider
    if (IsValid(Ctx.InstigatorActor))
    {
        if (const UActionCueProviderComponent* Prov = Ctx.InstigatorActor->FindComponentByClass<UActionCueProviderComponent>())
        {
            if (Prov->CueSet && Prov->CueSet->TryGetCue(CueTag, OutDef))
            {
                return true;
            }
        }
    }

    // 4) Global defaults
    const UActionCueSettings* Settings = GetDefault<UActionCueSettings>();
    if (Settings)
    {
        if (UActionCueSet* GlobalSet = LoadCueSetSoft(Settings->GlobalCueSet))
        {
            if (GlobalSet->TryGetCue(CueTag, OutDef))
            {
                return true;
            }
        }
    }

    return false;
}

uint64 UActionCueSubsystem::MakeCooldownKey(const FGameplayTag& CueTag, const FActionCueDef& Def, const FActionCueContext& Ctx) const
{
    // Cheap stable key:
    // tag hash + (optional instigator ptr)
    const uint32 TagHash = GetTypeHash(CueTag);

    uint64 Key = (uint64)TagHash;

    if (Def.bCooldownPerInstigator && IsValid(Ctx.InstigatorActor))
    {
        // fold pointer bits in
        const uint64 Ptr = (uint64)reinterpret_cast<uintptr_t>(Ctx.InstigatorActor.Get());
        Key ^= (Ptr + 0x9e3779b97f4a7c15ULL + (Key << 6) + (Key >> 2));
    }

    return Key;
}

bool UActionCueSubsystem::PassesCooldown(const FGameplayTag& CueTag, const FActionCueDef& Def, const FActionCueContext& Ctx) const
{
    if (Def.CooldownSeconds <= 0.f) return true;

    const double Now = GetWorld()->GetTimeSeconds();
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

    const uint64 Key = MakeCooldownKey(CueTag, Def, Ctx);
    LastPlayedTimeByKey.FindOrAdd(Key) = GetWorld()->GetTimeSeconds();
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
    // If no target: allow cues that don’t depend on target (you can change this policy)
    if (!IsValid(Ctx.TargetActor)) return true;

    // Your combat code already has IsDeadByAttributes(A).
    // Call it here if available. For now, we only guard “being destroyed”.
    if (Ctx.TargetActor->IsActorBeingDestroyed()) return false;

    // TODO: wire your real dead check:
    // if (IsDeadByAttributes(Ctx.TargetActor)) return false;

    return true;
}
