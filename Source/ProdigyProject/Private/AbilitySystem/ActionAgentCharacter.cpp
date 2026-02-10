#include "AbilitySystem/ActionAgentCharacter.h"

AActionAgentCharacter::AActionAgentCharacter()
{
	Status       = CreateDefaultSubobject<UStatusComponent>(TEXT("Status"));
	Attributes   = CreateDefaultSubobject<UAttributesComponent>(TEXT("Attributes"));
}

void AActionAgentCharacter::GetOwnedGameplayTags_Implementation(FGameplayTagContainer& OutTags) const
{
	OutTags.Reset();

	if (IsValid(Status))
	{
		Status->GetOwnedTags(OutTags);
	}
}

bool AActionAgentCharacter::HasAttribute_Implementation(FGameplayTag AttributeTag) const
{
	return IsValid(Attributes) ? Attributes->HasAttribute(AttributeTag) : false;
}

float AActionAgentCharacter::GetAttributeCurrentValue_Implementation(FGameplayTag AttributeTag) const
{
	return IsValid(Attributes) ? Attributes->GetCurrentValue(AttributeTag) : 0.f;
}

bool AActionAgentCharacter::ModifyAttributeCurrentValue_Implementation(FGameplayTag AttributeTag, float Delta, AActor* InstigatorActor)
{
	return IsValid(Attributes) ? Attributes->ModifyCurrentValue(AttributeTag, Delta, InstigatorActor) : false;
}

bool AActionAgentCharacter::AddStatusTag_Implementation(const FGameplayTag& StatusTag, int32 Turns, float Seconds, AActor* InstigatorActor)
{
	(void)InstigatorActor;
	return IsValid(Status) ? Status->AddStatusTag(StatusTag, Turns, Seconds) : false;
}
