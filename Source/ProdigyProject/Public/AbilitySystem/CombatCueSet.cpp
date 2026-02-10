#include "AbilitySystem/CombatCueSet.h"

bool UCombatCueSet::TryGetCueDef(FGameplayTag CueTag, FCombatCueDef& OutDef) const
{
	if (!CueTag.IsValid()) return false;

	if (const FCombatCueDef* Found = Cues.Find(CueTag))
	{
		OutDef = *Found;
		return true;
	}

	return false;
}
