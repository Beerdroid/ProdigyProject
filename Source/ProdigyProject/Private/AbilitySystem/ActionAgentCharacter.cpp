#include "AbilitySystem/ActionAgentCharacter.h"

AActionAgentCharacter::AActionAgentCharacter()
{
	TurnResource = CreateDefaultSubobject<UTurnResourceComponent>(TEXT("TurnResource"));
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Status = CreateDefaultSubobject<UStatusComponent>(TEXT("Status"));
}

void AActionAgentCharacter::GetOwnedGameplayTags_Implementation(FGameplayTagContainer& OutTags) const
{
	OutTags.Reset();

	if (IsValid(Status))
	{
		Status->GetOwnedTags(OutTags);
	}

	// Optional: add equipment/combat mode tags later from other systems here.
}

bool AActionAgentCharacter::HasActionPoints_Implementation(int32 Amount) const
{
	return IsValid(TurnResource) ? TurnResource->HasAP(Amount) : (Amount <= 0);
}

bool AActionAgentCharacter::SpendActionPoints_Implementation(int32 Amount)
{
	return IsValid(TurnResource) ? TurnResource->SpendAP(Amount) : (Amount <= 0);
}

bool AActionAgentCharacter::ApplyDamage_Implementation(float Amount, AActor* InstigatorActor)
{
	return IsValid(Health) ? Health->ApplyDamage(Amount, InstigatorActor) : false;
}

bool AActionAgentCharacter::AddStatusTag_Implementation(const FGameplayTag& StatusTag, int32 Turns, float Seconds, AActor* InstigatorActor)
{
	(void)InstigatorActor;
	return IsValid(Status) ? Status->AddStatusTag(StatusTag, Turns, Seconds) : false;
}
