
#include "Character/CombatantCharacterBase.h"

#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/StatusComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "BrainComponent.h"

ACombatantCharacterBase::ACombatantCharacterBase()
{
	Status       = CreateDefaultSubobject<UStatusComponent>(TEXT("Status"));
	ActionComponent = CreateDefaultSubobject<UActionComponent>(TEXT("ActionComponent"));
	Attributes = CreateDefaultSubobject<UAttributesComponent>(TEXT("Attributes"));
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

bool ACombatantCharacterBase::AddStatusTag_Implementation(const FGameplayTag& StatusTag, int32 Turns, float Seconds, AActor* InstigatorActor)
{
	(void)InstigatorActor;
	return Status ? Status->AddStatusTag(StatusTag, Turns, Seconds) : false;
}

bool ACombatantCharacterBase::HasAttribute_Implementation(FGameplayTag AttributeTag) const
{
	return IsValid(Attributes) ? Attributes->HasAttribute(AttributeTag) : false;
}

float ACombatantCharacterBase::GetAttributeCurrentValue_Implementation(FGameplayTag AttributeTag) const
{
	return IsValid(Attributes) ? Attributes->GetCurrentValue(AttributeTag) : 0.f;
}

bool ACombatantCharacterBase::ModifyAttributeCurrentValue_Implementation(FGameplayTag AttributeTag, float Delta, AActor* InstigatorActor)
{
	return IsValid(Attributes) ? Attributes->ModifyCurrentValue(AttributeTag, Delta, InstigatorActor) : false;
}

float ACombatantCharacterBase::GetAttributeFinalValue_Implementation(FGameplayTag AttributeTag) const
{
	return IsValid(Attributes) ? Attributes->GetFinalValue(AttributeTag) : 0.f;
}
