#include "AbilitySystem/AttributesComponent.h"

#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/AttributeSetDataAsset.h"
#include "AbilitySystem/ProdigyGameplayTags.h"

UAttributesComponent::UAttributesComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAttributesComponent::BeginPlay()
{
	Super::BeginPlay();
	BuildMapFromDefaults();
}

int32 UAttributesComponent::FindTurnEffectIndex(const FGameplayTag& EffectTag, const FGameplayTag& AttributeTag) const
{
	if (!EffectTag.IsValid() || !AttributeTag.IsValid()) return INDEX_NONE;

	for (int32 i = 0; i < TurnEffects.Num(); ++i)
	{
		const FPeriodicTurnEffect& E = TurnEffects[i];
		if (E.EffectTag.MatchesTagExact(EffectTag) && E.AttributeTag.MatchesTagExact(AttributeTag))
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UAttributesComponent::AppendDefaultsToMap(const TArray<FAttributeEntry>& InDefaults)
{
	for (const FAttributeEntry& E : InDefaults)
	{
		if (!E.AttributeTag.IsValid()) continue;

		// Last wins (intentional) to allow overrides in the SAME source
		AttributeMap.Add(E.AttributeTag, E);
	}
}

void UAttributesComponent::AppendDefaultsToMap_KeepCurrent(const TArray<FAttributeEntry>& InDefaults)
{
	for (const FAttributeEntry& D : InDefaults)
	{
		if (!D.AttributeTag.IsValid()) continue;

		if (FAttributeEntry* Existing = AttributeMap.Find(D.AttributeTag))
		{
			// Refresh base from data (balance changes, etc.) but KEEP runtime current.
			Existing->BaseValue = D.BaseValue;

			// Optional: if you want DA to ever force current, you need a flag in FAttributeEntry.
			continue;
		}

		// New attribute -> initialize Current from Base (first time only)
		FAttributeEntry NewE = D;
		NewE.CurrentValue = NewE.BaseValue;

		AttributeMap.Add(NewE.AttributeTag, NewE);
	}
}

void UAttributesComponent::BuildMapFromDefaults()
{
	if (!IsValid(AttributeSet))
	{
		UE_LOG(LogAttributes, Error,
			TEXT("BuildMapFromDefaults FAILED: AttributeSet is null on %s (Owner=%s)"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()));
		return;
	}

	if (bDefaultsInitialized)
	{
		UE_LOG(LogAttributes, Verbose,
			TEXT("BuildMapFromDefaults SKIP: already initialized Comp=%s (%p) Owner=%s MapNum=%d"),
			*GetNameSafe(this), this, *GetNameSafe(GetOwner()), AttributeMap.Num());
		return;
	}

	AttributeMap.Reset();
	AppendDefaultsToMap_KeepCurrent(AttributeSet->DefaultAttributes);

	// initialize Current from Base on first init (if that’s your policy)
	for (auto& Pair : AttributeMap)
	{
		Pair.Value.CurrentValue = Pair.Value.BaseValue;
	}

	bDefaultsInitialized = true;

	UE_LOG(LogAttributes, Log,
		TEXT("BuildMapFromDefaults OK: Owner=%s merged %d attributes from %s (FirstInit=1)"),
		*GetNameSafe(GetOwner()),
		AttributeMap.Num(),
		*GetNameSafe(AttributeSet));
}

bool UAttributesComponent::HasAttribute(FGameplayTag AttributeTag) const
{
	return AttributeTag.IsValid() && AttributeMap.Contains(AttributeTag);
}

const FAttributeEntry* UAttributesComponent::FindEntry(FGameplayTag AttributeTag) const
{
	if (!AttributeTag.IsValid()) return nullptr;
	return AttributeMap.Find(AttributeTag);
}

FAttributeEntry* UAttributesComponent::FindEntryMutable(FGameplayTag AttributeTag)
{
	if (!AttributeTag.IsValid()) return nullptr;
	return AttributeMap.Find(AttributeTag);
}

float UAttributesComponent::GetBaseValue(FGameplayTag AttributeTag) const
{
	if (const FAttributeEntry* E = FindEntry(AttributeTag))
	{
		return E->BaseValue;
	}
	return 0.f;
}

float UAttributesComponent::GetCurrentValue(FGameplayTag AttributeTag) const
{
	if (const FAttributeEntry* E = FindEntry(AttributeTag))
	{
		return E->CurrentValue;
	}
	return 0.f;
}

void UAttributesComponent::BroadcastChanged(const FGameplayTag Tag, float OldValue, float NewValue, AActor* InstigatorActor)
{
	if (!Tag.IsValid()) return;

	const float Delta = NewValue - OldValue;
	if (FMath::IsNearlyZero(Delta)) return;

	OnAttributeChanged.Broadcast(Tag, NewValue, Delta, InstigatorActor);
}

bool UAttributesComponent::SetBaseValue(FGameplayTag AttributeTag, float NewBaseValue, AActor* InstigatorActor)
{
	FAttributeEntry* E = FindEntryMutable(AttributeTag);
	if (!E)
	{
		UE_LOG(LogAttributes, Error, TEXT("SetBaseValue failed: missing attribute [%s] on %s"),
			*AttributeTag.ToString(), *GetNameSafe(GetOwner()));
		return false;
	}

	E->BaseValue = NewBaseValue;
	return true;
}

bool UAttributesComponent::SetCurrentValue(FGameplayTag AttributeTag, float NewCurrentValue, AActor* InstigatorActor)
{
	FAttributeEntry* E = FindEntryMutable(AttributeTag);
	if (!E)
	{
		UE_LOG(LogAttributes, Error, TEXT("SetCurrentValue failed: missing attribute [%s] on %s"),
			*AttributeTag.ToString(), *GetNameSafe(GetOwner()));
		return false;
	}

	const float Old = E->CurrentValue;
	E->CurrentValue = NewCurrentValue;

	BroadcastChanged(AttributeTag, Old, E->CurrentValue, InstigatorActor);
	return true;
}

bool UAttributesComponent::ModifyCurrentValue(FGameplayTag AttributeTag, float Delta, AActor* InstigatorActor)
{
	if (FMath::IsNearlyZero(Delta)) return true;

	FAttributeEntry* E = FindEntryMutable(AttributeTag);
	if (!E)
	{
		UE_LOG(LogAttributes, Error, TEXT("ModifyCurrentValue failed: missing attribute [%s] on %s (Delta=%.3f)"),
			*AttributeTag.ToString(), *GetNameSafe(GetOwner()), Delta);
		return false;
	}

	const float Old = E->CurrentValue;
	E->CurrentValue = Old + Delta;

	UE_LOG(LogActionExec, Warning,
	TEXT("[Attr:%s] %s: %.2f -> %.2f (Delta=%.2f) Inst=%s"),
	*GetNameSafe(GetOwner()),
	*AttributeTag.ToString(),
	Old,
	E->CurrentValue,
	Delta,
	*GetNameSafe(InstigatorActor)
);

	BroadcastChanged(AttributeTag, Old, E->CurrentValue, InstigatorActor);
	return true;
}

bool UAttributesComponent::CopyCurrentValue(FGameplayTag FromTag, FGameplayTag ToTag, AActor* InstigatorActor)
{
	const FAttributeEntry* From = FindEntry(FromTag);
	FAttributeEntry* To = FindEntryMutable(ToTag);
	if (!From || !To) return false;

	const float Old = To->CurrentValue;
	To->CurrentValue = From->CurrentValue;

	BroadcastChanged(ToTag, Old, To->CurrentValue, InstigatorActor);
	return true;
}

static void ApplyModsToValue(float& InOutValue, const TArray<FAttributeMod>& Mods, const FGameplayTag& TargetTag)
{
	// Very simple order:
	// 1) Add
	// 2) Multiply
	// 3) Override (last wins)
	float AddSum = 0.f;
	float Mul = 1.f;
	bool bHasOverride = false;
	float OverrideValue = 0.f;

	for (const FAttributeMod& M : Mods)
	{
		if (!M.AttributeTag.MatchesTagExact(TargetTag)) continue;

		switch (M.Op)
		{
		case EAttrModOp::Add:      AddSum += M.Magnitude; break;
		case EAttrModOp::Multiply: Mul *= M.Magnitude; break;
		case EAttrModOp::Override: bHasOverride = true; OverrideValue = M.Magnitude; break;
		default: break;
		}
	}

	InOutValue = (InOutValue + AddSum) * Mul;
	if (bHasOverride)
	{
		InOutValue = OverrideValue;
	}
}

float UAttributesComponent::GetFinalValue(FGameplayTag AttributeTag) const
{
	const FAttributeEntry* E = FindEntry(AttributeTag);
	if (!E) return 0.f;

	float V = E->BaseValue;

	for (const auto& Pair : ModSources)
	{
		const FAttrModSource& Source = Pair.Value;
		ApplyModsToValue(V, Source.Mods, AttributeTag);
	}

	return V;
}

void UAttributesComponent::SetModsForSource(UObject* Source, const TArray<FAttributeMod>& Mods, UObject* InstigatorSource)
{
	if (!IsValid(Source))
	{
		UE_LOG(LogAttributes, Warning, TEXT("[Mods] SetModsForSource: Source invalid"));
		return;
	}

	FAttrModSource& S = ModSources.FindOrAdd(Source);
	S.Mods = Mods;

	for (const FAttributeMod& M : Mods)
	{
		UE_LOG(LogAttributes, Log, TEXT("[Mods]  Tag=%s Op=%d Mag=%.2f"),
			*M.AttributeTag.ToString(),
			(int32)M.Op,
			M.Magnitude);
	}

	UE_LOG(LogAttributes, Log, TEXT("[Mods] Set Source=%s Mods=%d"),
		*GetNameSafe(Source), Mods.Num());

	ReClampAllRelevantCurrents(InstigatorSource);
}

void UAttributesComponent::ClearModsForSource(UObject* Source, UObject* InstigatorSource)
{
	if (!IsValid(Source))
	{
		UE_LOG(LogAttributes, Warning, TEXT("[Mods] ClearModsForSource: Source invalid"));
		return;
	}

	const int32 Removed = ModSources.Remove(Source);

	UE_LOG(LogAttributes, Log, TEXT("[Mods] Clear Source=%s Removed=%d"),
		*GetNameSafe(Source), Removed);

	ReClampAllRelevantCurrents(InstigatorSource);
}

bool UAttributesComponent::ApplyItemAttributeModsAsCurrentDeltas(const TArray<FAttributeMod>& ItemMods,
	AActor* InstigatorActor)
{
	if (ItemMods.Num() == 0) return true;

	bool bAppliedAny = false;

	for (const FAttributeMod& M : ItemMods)
	{
		if (!M.AttributeTag.IsValid()) continue;

		// Consume/potion policy: only Add works as "delta to current"
		if (M.Op != EAttrModOp::Add)
		{
			UE_LOG(LogAttributes, Warning,
				TEXT("[ConsumeMods] Skipping non-Add mod Tag=%s Op=%d Mag=%.2f"),
				*M.AttributeTag.ToString(), (int32)M.Op, M.Magnitude);
			continue;
		}

		// No magic: attribute must exist in the set
		if (!HasAttribute(M.AttributeTag))
		{
			UE_LOG(LogAttributes, Warning,
				TEXT("[ConsumeMods] Missing attribute Tag=%s on %s"),
				*M.AttributeTag.ToString(), *GetNameSafe(GetOwner()));
			continue;
		}

		if (FMath::IsNearlyZero(M.Magnitude))
		{
			UE_LOG(LogAttributes, Verbose,
				TEXT("[ConsumeMods] Zero delta ignored Tag=%s"),
				*M.AttributeTag.ToString());
			continue;
		}

		const float OldCur = GetCurrentValue(M.AttributeTag);

		const bool bOk = ModifyCurrentValue(M.AttributeTag, M.Magnitude, InstigatorActor);

		const float NewCur = GetCurrentValue(M.AttributeTag);

		UE_LOG(LogAttributes, Warning,
			TEXT("[ConsumeMods]  + Tag=%s Cur %.2f -> %.2f (Delta=%.2f) Ok=%d"),
			*M.AttributeTag.ToString(), OldCur, NewCur, M.Magnitude, bOk ? 1 : 0);

		bAppliedAny |= bOk;
	}

	// Clamp once (health/ap/mana etc.) after all changes
	if (bAppliedAny)
	{
		ClampResourcesIfNeeded(InstigatorActor);
	}

	return bAppliedAny;
}

void UAttributesComponent::ReClampAllRelevantCurrents(UObject* InstigatorSource)
{
	ClampResourcesIfNeeded(InstigatorSource);
}

void UAttributesComponent::ClampResourcesIfNeeded(UObject* InstigatorSource)
{
	AActor* InstigatorActor = Cast<AActor>(InstigatorSource);

	// If no asset, do nothing (you can keep old hardcoded behavior if you prefer,
	// but this version is fully data-driven).
	if (!IsValid(AttributeSet))
	{
		return;
	}

	for (const FProdigyResourcePair& Pair : AttributeSet->ResourcePairs)
	{
		if (!Pair.CurrentTag.IsValid() || !Pair.MaxTag.IsValid()) continue;

		// Must exist explicitly (no magic)
		if (!HasAttribute(Pair.CurrentTag)) continue;
		if (!HasAttribute(Pair.MaxTag)) continue;

		const float MaxV = GetFinalValue(Pair.MaxTag);
		const float CurV = GetCurrentValue(Pair.CurrentTag);
		const float Clamped = FMath::Clamp(CurV, 0.f, MaxV);

		if (!FMath::IsNearlyEqual(CurV, Clamped))
		{
			UE_LOG(LogAttributes, Warning,
				TEXT("[Clamp] %s %s %.1f -> %.1f (Max(%s)=%.1f) Inst=%s"),
				*GetNameSafe(GetOwner()),
				*Pair.CurrentTag.ToString(),
				CurV,
				Clamped,
				*Pair.MaxTag.ToString(),
				MaxV,
				*GetNameSafe(InstigatorActor));

			// Keep your existing broadcast behavior
			SetCurrentValue(Pair.CurrentTag, Clamped, InstigatorActor);
		}
		else
		{
			UE_LOG(LogAttributes, Verbose,
				TEXT("[Clamp] %s %s OK %.1f / %.1f"),
				*GetNameSafe(GetOwner()),
				*Pair.CurrentTag.ToString(),
				CurV,
				MaxV);
		}
	}
}

bool UAttributesComponent::AddOrRefreshTurnEffect(
	const FGameplayTag& EffectTag,
	const FGameplayTag& AttributeTag,
	float DeltaPerTurn,
	int32 NumTurns,
	AActor* InstigatorActor,
	bool bRefreshDuration,
	bool bStackMagnitude)
{
	if (!EffectTag.IsValid() || !AttributeTag.IsValid()) return false;
	if (NumTurns <= 0) return false;
	if (FMath::IsNearlyZero(DeltaPerTurn)) return false;

	// No magic: must be a known attribute from your AttributeSet->DefaultAttributes (i.e., AttributeMap)
	if (!HasAttribute(AttributeTag))
	{
		UE_LOG(LogAttributes, Warning,
			TEXT("[TurnEffect] Missing attribute %s on %s (Effect=%s)"),
			*AttributeTag.ToString(), *GetNameSafe(GetOwner()), *EffectTag.ToString());
		return false;
	}

	const int32 Idx = FindTurnEffectIndex(EffectTag, AttributeTag);
	if (Idx != INDEX_NONE)
	{
		FPeriodicTurnEffect& E = TurnEffects[Idx];

		if (bRefreshDuration)
		{
			// “Refresh” normally means set/extend duration
			E.TurnsRemaining = FMath::Max(E.TurnsRemaining, NumTurns);
		}

		if (bStackMagnitude)
		{
			E.DeltaPerTurn += DeltaPerTurn;
		}
		else
		{
			E.DeltaPerTurn = DeltaPerTurn;
		}

		E.InstigatorActor = InstigatorActor;

		UE_LOG(LogAttributes, Warning,
			TEXT("[TurnEffect] Refresh Effect=%s Attr=%s Delta=%.2f Turns=%d Owner=%s"),
			*EffectTag.ToString(), *AttributeTag.ToString(), E.DeltaPerTurn, E.TurnsRemaining, *GetNameSafe(GetOwner()));

		return true;
	}

	FPeriodicTurnEffect NewE;
	NewE.EffectTag = EffectTag;
	NewE.AttributeTag = AttributeTag;
	NewE.DeltaPerTurn = DeltaPerTurn;
	NewE.TurnsRemaining = NumTurns;
	NewE.InstigatorActor = InstigatorActor;

	TurnEffects.Add(NewE);

	UE_LOG(LogAttributes, Warning,
		TEXT("[TurnEffect] Add Effect=%s Attr=%s Delta=%.2f Turns=%d Owner=%s"),
		*EffectTag.ToString(), *AttributeTag.ToString(), DeltaPerTurn, NumTurns, *GetNameSafe(GetOwner()));

	return true;
}

void UAttributesComponent::TickTurnEffects(AActor* OwnerTurnActor)
{
	if (TurnEffects.Num() == 0) return;

	// Tick reverse to allow RemoveAtSwap
	for (int32 i = TurnEffects.Num() - 1; i >= 0; --i)
	{
		FPeriodicTurnEffect& E = TurnEffects[i];

		// Purge invalid/expired entries
		if (!E.EffectTag.IsValid() || !E.AttributeTag.IsValid() || E.TurnsRemaining <= 0 || FMath::IsNearlyZero(E.DeltaPerTurn))
		{
			TurnEffects.RemoveAtSwap(i);
			continue;
		}

		// Attribute must still exist
		if (!HasAttribute(E.AttributeTag))
		{
			UE_LOG(LogAttributes, Warning,
				TEXT("[TurnEffect] Remove (attr missing) Effect=%s Attr=%s Owner=%s"),
				*E.EffectTag.ToString(), *E.AttributeTag.ToString(), *GetNameSafe(GetOwner()));
			TurnEffects.RemoveAtSwap(i);
			continue;
		}

		const float OldCur = GetCurrentValue(E.AttributeTag);

		// IMPORTANT: instigator for logs/rules = whoever is acting this turn (owner), not who applied it
		// But you still keep E.InstigatorActor stored for future “dispel by source” rules.
		const bool bOk = ModifyCurrentValue(E.AttributeTag, E.DeltaPerTurn, OwnerTurnActor);
		const float NewCur = GetCurrentValue(E.AttributeTag);

		UE_LOG(LogAttributes, Warning,
			TEXT("[TurnEffect] Tick Effect=%s Attr=%s %.2f -> %.2f (Delta=%.2f) TurnsLeft(before)=%d Ok=%d Owner=%s"),
			*E.EffectTag.ToString(),
			*E.AttributeTag.ToString(),
			OldCur, NewCur,
			E.DeltaPerTurn,
			E.TurnsRemaining,
			bOk ? 1 : 0,
			*GetNameSafe(GetOwner()));

		// Your existing resource clamp (Health->MaxHealth, Mana->MaxMana etc. via DA ResourcePairs)
		ClampResourcesIfNeeded(OwnerTurnActor);

		// Decrement and expire
		E.TurnsRemaining -= 1;
		if (E.TurnsRemaining <= 0)
		{
			UE_LOG(LogAttributes, Warning,
				TEXT("[TurnEffect] Expired Effect=%s Attr=%s Owner=%s"),
				*E.EffectTag.ToString(), *E.AttributeTag.ToString(), *GetNameSafe(GetOwner()));

			TurnEffects.RemoveAtSwap(i);
		}
	}
}

void UAttributesComponent::ClearTurnEffect(FGameplayTag EffectTag)
{
	if (!EffectTag.IsValid()) return;

	for (int32 i = TurnEffects.Num() - 1; i >= 0; --i)
	{
		if (TurnEffects[i].EffectTag.MatchesTagExact(EffectTag))
		{
			TurnEffects.RemoveAtSwap(i);
		}
	}
}