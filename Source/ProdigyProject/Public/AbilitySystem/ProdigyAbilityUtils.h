#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ProdigyGameplayTags.h"
#include "AbilitySystem/ActionAgentInterface.h"

namespace ProdigyAbilityUtils
{
	static FORCEINLINE bool IsDeadByAttributes(AActor* A)
	{
		if (!IsValid(A)) return true;
		if (!A->GetClass()->ImplementsInterface(UActionAgentInterface::StaticClass())) return false;

		const float HP = IActionAgentInterface::Execute_GetAttributeCurrentValue(A, ProdigyTags::Attr::Health);
		return HP <= 0.f;
	}
}
