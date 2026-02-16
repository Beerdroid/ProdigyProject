#include "AbilitySystem/AttributesComponent.h"

#include "AbilitySystem/ActionComponent.h"
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

void UAttributesComponent::BuildMapFromDefaults()
{
	AttributeMap.Reset();

	for (const FAttributeEntry& E : DefaultAttributes)
	{
		if (!E.AttributeTag.IsValid()) continue;

		// Last wins if duplicates exist (intentional, to allow BP overrides)
		AttributeMap.Add(E.AttributeTag, E);
	}
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

void UAttributesComponent::ReClampAllRelevantCurrents(UObject* InstigatorSource)
{
	ClampResourcesIfNeeded(InstigatorSource);
}

void UAttributesComponent::ClampResourcesIfNeeded(UObject* InstigatorSource)
{
	AActor* InstigatorActor = Cast<AActor>(InstigatorSource);

	// =========================
	// HEALTH
	// =========================
	if (HasAttribute(ProdigyTags::Attr::Health))
	{
		const float MaxH = GetFinalValue(ProdigyTags::Attr::MaxHealth);
		const float CurH = GetCurrentValue(ProdigyTags::Attr::Health);
		const float Clamped = FMath::Clamp(CurH, 0.f, MaxH);

		if (!FMath::IsNearlyEqual(CurH, Clamped))
		{
			UE_LOG(LogAttributes, Warning,
				TEXT("[Clamp] %s Health %.1f -> %.1f (Max=%.1f) Inst=%s"),
				*GetNameSafe(GetOwner()),
				CurH,
				Clamped,
				MaxH,
				*GetNameSafe(InstigatorActor));

			SetCurrentValue(ProdigyTags::Attr::Health, Clamped, InstigatorActor);
		}
		else
		{
			UE_LOG(LogAttributes, Verbose,
				TEXT("[Clamp] %s Health OK %.1f / %.1f"),
				*GetNameSafe(GetOwner()),
				CurH,
				MaxH);
		}
	}

	// =========================
	// AP
	// =========================
	if (HasAttribute(ProdigyTags::Attr::AP))
	{
		const float MaxAP = GetFinalValue(ProdigyTags::Attr::MaxAP);
		const float CurAP = GetCurrentValue(ProdigyTags::Attr::AP);
		const float Clamped = FMath::Clamp(CurAP, 0.f, MaxAP);

		if (!FMath::IsNearlyEqual(CurAP, Clamped))
		{
			UE_LOG(LogAttributes, Warning,
				TEXT("[Clamp] %s AP %.1f -> %.1f (Max=%.1f) Inst=%s"),
				*GetNameSafe(GetOwner()),
				CurAP,
				Clamped,
				MaxAP,
				*GetNameSafe(InstigatorActor));

			SetCurrentValue(ProdigyTags::Attr::AP, Clamped, InstigatorActor);
		}
		else
		{
			UE_LOG(LogAttributes, Verbose,
				TEXT("[Clamp] %s AP OK %.1f / %.1f"),
				*GetNameSafe(GetOwner()),
				CurAP,
				MaxAP);
		}
	}
}


