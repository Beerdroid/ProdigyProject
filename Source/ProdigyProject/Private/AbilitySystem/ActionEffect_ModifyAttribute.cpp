#include "AbilitySystem/ActionEffect_ModifyAttribute.h"

#include "AbilitySystem/ActionAgentInterface.h"

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
	return IActionAgentInterface::Execute_ModifyAttributeCurrentValue(A, AttributeTag, FinalDelta, Context.Instigator);
}
