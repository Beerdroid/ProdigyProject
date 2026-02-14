#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ActionCueTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ActionCueSet.generated.h"

USTRUCT(BlueprintType)
struct FWeaponLayerEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag WeaponTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> Sound = nullptr;
};

USTRUCT(BlueprintType)
struct FSurfaceLayerEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag SurfaceTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> Sound = nullptr;
};

USTRUCT(BlueprintType)
struct FActionCueSurfaceTagMapEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UPhysicalMaterial> PhysicalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag SurfaceTag;
};

USTRUCT(BlueprintType)
struct FCueWeaponLayerSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag CueTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FWeaponLayerEntry> WeaponLayers;
};

USTRUCT(BlueprintType)
struct FCueSurfaceLayerSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag CueTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FSurfaceLayerEntry> SurfaceLayers;
};

UCLASS(BlueprintType)
class PRODIGYPROJECT_API UActionCueSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, FActionCueDef> Cues;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MetaSound|Layers")
	TArray<FCueWeaponLayerSet> WeaponLayerSets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MetaSound|Layers")
	TArray<FCueSurfaceLayerSet> SurfaceLayerSets;

	UFUNCTION(BlueprintCallable)
	bool TryGetCue(const FGameplayTag& CueTag, FActionCueDef& OutDef) const
	{
		if (const FActionCueDef* Found = Cues.Find(CueTag))
		{
			OutDef = *Found;
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintCallable)
	void ResolveMetaSoundsForContext(
		const FGameplayTag& CueTag,
		const FGameplayTag& WeaponTag,
		const FGameplayTag& SurfaceTag,
		USoundBase*& OutWeaponSound,
		USoundBase*& OutSurfaceSound) const;
};
