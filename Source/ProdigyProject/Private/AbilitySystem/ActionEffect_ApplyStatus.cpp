#include "AbilitySystem/ActionEffect_ApplyStatus.h"
#include "AbilitySystem/ActionAgentInterface.h"

bool UActionEffect_ApplyStatus::Apply_Implementation(const FActionContext& Context) const
{
	if (!IsValid(Context.TargetActor) || !StatusTag.IsValid()) return false;

	if (Context.TargetActor->GetClass()->ImplementsInterface(UActionAgentInterface::StaticClass()))
	{
		return IActionAgentInterface::Execute_AddStatusTag(Context.TargetActor, StatusTag, Turns, Seconds, Context.Instigator);
	}

	return false;
}
