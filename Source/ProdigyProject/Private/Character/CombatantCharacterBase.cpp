
#include "Character/CombatantCharacterBase.h"

#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/TurnResourceComponent.h"
#include "AbilitySystem/HealthComponent.h"
#include "AbilitySystem/StatusComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "BrainComponent.h"

ACombatantCharacterBase::ACombatantCharacterBase()
{
	TurnResource = CreateDefaultSubobject<UTurnResourceComponent>(TEXT("TurnResource"));
	Health       = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Status       = CreateDefaultSubobject<UStatusComponent>(TEXT("Status"));
	ActionComponent = CreateDefaultSubobject<UActionComponent>(TEXT("ActionComponent"));
}

bool ACombatantCharacterBase::IsAlive_Implementation() const
{
	return Health ? (Health->CurrentHealth > 0.f) : true;
}

void ACombatantCharacterBase::OnCombatFreeze_Implementation(bool bFrozen)
{
	// Snap-freeze gameplay only; keep visuals alive.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (bFrozen)
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}
		else
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	// Pause AI decision-making (if AI controlled)
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (bFrozen)
		{
			AIC->StopMovement();
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("CombatFreeze"));
			}
		}
		else
		{
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->RestartLogic();
			}
		}
	}
}

// ---- IActionAgentInterface ----

void ACombatantCharacterBase::GetOwnedGameplayTags_Implementation(FGameplayTagContainer& OutTags) const
{
	OutTags.Reset();
	if (Status)
	{
		Status->GetOwnedTags(OutTags);
	}
}

bool ACombatantCharacterBase::HasActionPoints_Implementation(int32 Amount) const
{
	return TurnResource ? TurnResource->HasAP(Amount) : (Amount <= 0);
}

bool ACombatantCharacterBase::SpendActionPoints_Implementation(int32 Amount)
{
	return TurnResource ? TurnResource->SpendAP(Amount) : (Amount <= 0);
}

bool ACombatantCharacterBase::ApplyDamage_Implementation(float Amount, AActor* InstigatorActor)
{
	return Health ? Health->ApplyDamage(Amount, InstigatorActor) : false;
}

bool ACombatantCharacterBase::AddStatusTag_Implementation(const FGameplayTag& StatusTag, int32 Turns, float Seconds, AActor* InstigatorActor)
{
	(void)InstigatorActor;
	return Status ? Status->AddStatusTag(StatusTag, Turns, Seconds) : false;
}
