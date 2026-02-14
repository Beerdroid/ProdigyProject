#include "AbilitySystem/AttributesComponent.h"

#include "AbilitySystem/ActionComponent.h"

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
