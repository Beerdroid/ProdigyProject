#include "AbilitySystem/ActionEffect_DealDamage.h"
#include "AbilitySystem/ActionAgentInterface.h"

bool UActionEffect_DealDamage::Apply_Implementation(const FActionContext& Context) const
{
	if (!IsValid(Context.TargetActor)) return false;

	if (Context.TargetActor->GetClass()->ImplementsInterface(UActionAgentInterface::StaticClass()))
	{
		return false;
	}

	return false;
}
