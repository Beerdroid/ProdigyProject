#include "AbilitySystem/ActionCueSet.h"

void UActionCueSet::ResolveMetaSoundsForContext(const FGameplayTag& CueTag,
                                                const FGameplayTag& WeaponTag,
                                                const FGameplayTag& SurfaceTag,
                                                USoundBase*& OutWeaponSound,
                                                USoundBase*& OutSurfaceSound) const
{
	OutWeaponSound = nullptr;
	OutSurfaceSound = nullptr;

	for (const FCueWeaponLayerSet& Set : WeaponLayerSets)
	{
		if (Set.CueTag != CueTag) continue;

		for (const FWeaponLayerEntry& Entry : Set.WeaponLayers)
		{
			if (Entry.WeaponTag == WeaponTag)
			{
				OutWeaponSound = Entry.Sound;
				break;
			}
		}
		break;
	}

	for (const FCueSurfaceLayerSet& Set : SurfaceLayerSets)
	{
		if (Set.CueTag != CueTag) continue;

		for (const FSurfaceLayerEntry& Entry : Set.SurfaceLayers)
		{
			if (Entry.SurfaceTag == SurfaceTag)
			{
				OutSurfaceSound = Entry.Sound;
				break;
			}
		}
		break;
	}
}
