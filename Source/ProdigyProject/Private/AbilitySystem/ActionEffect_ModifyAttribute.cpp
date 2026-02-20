#include "AbilitySystem/ActionEffect_ModifyAttribute.h"

#include "AbilitySystem/ActionAgentInterface.h"
#include "AbilitySystem/ProdigyGameplayTags.h"
#include "AbilitySystem/WorldCombatEvents.h"


AActor* UActionEffect_ModifyAttribute::ResolveTargetActor(const FActionContext& Context) const
{
	switch (Target)
	{
	case EActionEffectTarget::Instigator:
		return Context.Instigator;
	case EActionEffectTarget::Target:
	default:
		return Context.TargetActor;
	}
}

bool UActionEffect_ModifyAttribute::Apply_Implementation(const FActionContext& Context) const
{
	if (!AttributeTag.IsValid()) return false;
	if (FMath::IsNearlyZero(Delta)) return true;

	AActor* A = ResolveTargetActor(Context);
	if (!IsValid(A)) return false;

	if (!A->GetClass()->ImplementsInterface(UActionAgentInterface::StaticClass()))
	{
		return false;
	}

	// Must exist (explicit, no magic)
	const bool bHasAttr = IActionAgentInterface::Execute_HasAttribute(A, AttributeTag);
	if (!bHasAttr)
	{
		return false;
	}

	const float OldValue = IActionAgentInterface::Execute_GetAttributeCurrentValue(A, AttributeTag);
	float NewValue = OldValue + Delta;

	// Explicit clamp options
	if (bClampMinZero)
	{
		NewValue = FMath::Max(0.f, NewValue);
	}

	if (bClampToMaxAttribute)
	{
		if (!MaxAttributeTag.IsValid())
		{
			return false;
		}

		if (!IActionAgentInterface::Execute_HasAttribute(A, MaxAttributeTag))
		{
			return false;
		}

		const float MaxV = IActionAgentInterface::Execute_GetAttributeCurrentValue(A, MaxAttributeTag);
		NewValue = FMath::Min(NewValue, MaxV);
	}

	// Apply as delta (so attribute component broadcasts correctly)
	const float FinalDelta = NewValue - OldValue;

	if (FMath::IsNearlyZero(FinalDelta))
	{
		return true;
	}

	const bool bOk = IActionAgentInterface::Execute_ModifyAttributeCurrentValue(
		A, AttributeTag, FinalDelta, Context.Instigator);

	if (!bOk) return false;

	// --- World events for floating text (Health only) ---
	if (AttributeTag.MatchesTagExact(ProdigyTags::Attr::Health))
	{
		UWorld* World = A->GetWorld();
		if (World)
		{
			if (UWorldCombatEvents* Events = World->GetSubsystem<UWorldCombatEvents>())
			{
				const float NewHP = IActionAgentInterface::Execute_GetAttributeCurrentValue(A, AttributeTag);

				// Positive delta = heal
				if (FinalDelta > 0.f)
				{
					Events->OnWorldHealEvent.Broadcast(
						A,
						Context.Instigator,
						FinalDelta,   // AppliedHeal
						OldValue,     // OldHP
						NewHP         // NewHP
					);
				}
				// Negative delta = damage
				else
				{
					const float AppliedDamage = FMath::Abs(FinalDelta);

					Events->OnWorldDamageEvent.Broadcast(
						A,
						Context.Instigator,
						AppliedDamage,
						OldValue,
						NewHP
					);
				}
			}
		}
	}

	return true;
}