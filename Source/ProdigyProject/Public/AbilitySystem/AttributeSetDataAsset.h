#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AttributesComponent.h" // for FAttributeEntry
#include "AttributeSetDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FProdigyResourcePair
{
	GENERATED_BODY()

	// Current value attribute tag (e.g. Attr.Health, Attr.Mana, Attr.AP)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	FGameplayTag CurrentTag;

	// Max/cap attribute tag (e.g. Attr.MaxHealth, Attr.MaxMana, Attr.MaxAP)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	FGameplayTag MaxTag;
};

UCLASS(BlueprintType)
class PRODIGYPROJECT_API UAttributeSetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// Default attributes for this set (Player, Enemy, Boss, etc.)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	TArray<FAttributeEntry> DefaultAttributes;

	// Which "current" attributes should be clamped by which "max" attributes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attributes")
	TArray<FProdigyResourcePair> ResourcePairs;
};
