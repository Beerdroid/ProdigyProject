#include "AbilitySystem/HealthComponent.h"

#include "AbilitySystem/AttributesComponent.h"
#include "AbilitySystem/ProdigyGameplayTags.h"

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	EnsureAttributes();

	// Initialize attributes if missing
	if (Attributes)
	{
		const FGameplayTag HealthTag    = ProdigyTags::Attr::Health;
		const FGameplayTag MaxHealthTag = ProdigyTags::Attr::MaxHealth;

		// If tags aren't present, we can't add them at runtime in v1 (no API for that),
		// so require they exist in DefaultAttributes. We will still sync if present.
		SyncFromAttributes();

		// Bind once
		if (!bBoundToAttributes)
		{
			Attributes->OnAttributeChanged.AddDynamic(this, &UHealthComponent::HandleAttributeChanged);
			bBoundToAttributes = true;
		}
	}
}

void UHealthComponent::EnsureAttributes()
{
	if (Attributes) return;

	if (AActor* OwnerActor = GetOwner())
	{
		Attributes = OwnerActor->FindComponentByClass<UAttributesComponent>();
	}
}

void UHealthComponent::SyncFromAttributes()
{
	if (!Attributes) return;

	const FGameplayTag HealthTag    = ProdigyTags::Attr::Health;
	const FGameplayTag MaxHealthTag = ProdigyTags::Attr::MaxHealth;

	if (Attributes->HasAttribute(MaxHealthTag))
	{
		MaxHealth = Attributes->GetCurrentValue(MaxHealthTag);
	}

	if (Attributes->HasAttribute(HealthTag))
	{
		CurrentHealth = Attributes->GetCurrentValue(HealthTag);
	}
}

void UHealthComponent::HandleAttributeChanged(FGameplayTag AttributeTag, float NewValue, float Delta, AActor* InstigatorActor)
{
	if (!AttributeTag.IsValid()) return;

	const FGameplayTag HealthTag    = ProdigyTags::Attr::Health;
	const FGameplayTag MaxHealthTag = ProdigyTags::Attr::MaxHealth;

	if (AttributeTag == MaxHealthTag)
	{
		MaxHealth = NewValue;
		return;
	}

	if (AttributeTag != HealthTag)
	{
		return;
	}

	// Update cached value and broadcast legacy delegates
	CurrentHealth = NewValue;
	OnHealthChanged.Broadcast(CurrentHealth, Delta, InstigatorActor);

	if (!bHasDied && CurrentHealth <= 0.f)
	{
		bHasDied = true;
		OnDied.Broadcast(InstigatorActor);
	}
}

void UHealthComponent::SetHealth(float NewHealth, AActor* InstigatorActor)
{
	EnsureAttributes();
	if (!Attributes) return;

	const FGameplayTag HealthTag    = ProdigyTags::Attr::Health;
	const FGameplayTag MaxHealthTag = ProdigyTags::Attr::MaxHealth;

	// If MaxHealth exists, clamp to it.
	float Clamped = NewHealth;
	if (Attributes->HasAttribute(MaxHealthTag))
	{
		const float MH = Attributes->GetCurrentValue(MaxHealthTag);
		Clamped = FMath::Clamp(NewHealth, 0.f, MH);
	}
	else
	{
		Clamped = FMath::Max(0.f, NewHealth);
	}

	Attributes->SetCurrentValue(HealthTag, Clamped, InstigatorActor);
}

bool UHealthComponent::ApplyDamage(float Amount, AActor* InstigatorActor)
{
	if (Amount <= 0.f) return false;

	EnsureAttributes();
	if (!Attributes) return false;

	const FGameplayTag HealthTag = ProdigyTags::Attr::Health;

	// Damage = negative delta
	return Attributes->ModifyCurrentValue(HealthTag, -Amount, InstigatorActor);
}
