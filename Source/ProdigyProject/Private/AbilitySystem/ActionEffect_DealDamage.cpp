#include "AbilitySystem/ActionEffect_DealDamage.h"
#include "AbilitySystem/ActionAgentInterface.h"
#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/ProdigyGameplayTags.h"

bool UActionEffect_DealDamage::Apply_Implementation(const FActionContext& Context) const
{
	if (!IsValid(Context.TargetActor)) return false;
	if (!IsValid(Context.Instigator)) return false;

	// Default to Attr.Health if not set explicitly (if you DON'T want this "default", tell me and I'll remove it)
	const FGameplayTag HealthTag = HealthAttributeTag.IsValid()
		? HealthAttributeTag
		: ProdigyTags::Attr::Health;

	if (!Context.TargetActor->GetClass()->ImplementsInterface(UActionAgentInterface::StaticClass()))
	{
		UE_LOG(LogActionExec, Error,
			TEXT("[DealDamage] Target %s missing ActionAgentInterface"),
			*GetNameSafe(Context.TargetActor));
		return false;
	}

	const bool bHasHealth = IActionAgentInterface::Execute_HasAttribute(Context.TargetActor, HealthTag);
	if (!bHasHealth)
	{
		UE_LOG(LogActionExec, Error,
			TEXT("[DealDamage] Target %s missing attribute %s in DefaultAttributes"),
			*GetNameSafe(Context.TargetActor),
			*HealthTag.ToString());
		return false;
	}

	const float OldHP = IActionAgentInterface::Execute_GetAttributeCurrentValue(Context.TargetActor, HealthTag);
	const float Delta = -FMath::Max(0.f, Damage);

	bool bOk = IActionAgentInterface::Execute_ModifyAttributeCurrentValue(Context.TargetActor, HealthTag, Delta, Context.Instigator);
	if (!bOk) return false;

	if (bClampMinZero)
	{
		const float NewHP = IActionAgentInterface::Execute_GetAttributeCurrentValue(Context.TargetActor, HealthTag);
		if (NewHP < 0.f)
		{
			// Clamp explicitly via delta to keep broadcast consistent
			const float ClampDelta = 0.f - NewHP;
			(void)IActionAgentInterface::Execute_ModifyAttributeCurrentValue(Context.TargetActor, HealthTag, ClampDelta, Context.Instigator);
		}
	}

	const float FinalHP = IActionAgentInterface::Execute_GetAttributeCurrentValue(Context.TargetActor, HealthTag);

	UE_LOG(LogActionExec, Warning,
		TEXT("[DealDamage] %s HP %.2f -> %.2f (Damage=%.2f) Inst=%s"),
		*GetNameSafe(Context.TargetActor),
		OldHP,
		FinalHP,
		Damage,
		*GetNameSafe(Context.Instigator));

	return true;
}
