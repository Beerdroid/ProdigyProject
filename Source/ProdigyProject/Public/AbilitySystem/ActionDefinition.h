#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ActionTypes.h"
#include "ActionEffect.h"
#include "ActionDefinition.generated.h"

USTRUCT(BlueprintType)
struct FActionCostCombat
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 APCost = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 CooldownTurns = 0;
};

USTRUCT(BlueprintType)
struct FActionCostExploration
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float CooldownSeconds = 0.f;
};

UCLASS(BlueprintType)
class PRODIGYPROJECT_API UActionDefinition : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|UI")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|UI")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|UI")
	TObjectPtr<UTexture2D> Icon = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action")
	FGameplayTag ActionTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action")
	EActionTargetingMode TargetingMode = EActionTargetingMode::Unit;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Availability")
	bool bUsableInExploration = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Availability")
	bool bUsableInCombat = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Requirements")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Requirements")
	FGameplayTagContainer BlockedTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Costs")
	FActionCostCombat Combat;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Action|Costs")
	FActionCostExploration Exploration;

	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category="Action|Effects")
	TArray<TObjectPtr<UActionEffect>> Effects;
};
