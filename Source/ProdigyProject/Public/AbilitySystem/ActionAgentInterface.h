#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "ActionAgentInterface.generated.h"

UINTERFACE(BlueprintType)
class UActionAgentInterface : public UInterface
{
	GENERATED_BODY()
};

class PRODIGYPROJECT_API IActionAgentInterface
{
	GENERATED_BODY()

public:
	// Tags used for gating actions (stunned, silenced, etc.)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Action|Agent")
	void GetOwnedGameplayTags(FGameplayTagContainer& OutTags) const;

	// --- Generic Attributes (new) ---
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Action|Agent|Attributes")
	bool HasAttribute(FGameplayTag AttributeTag) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Action|Agent|Attributes")
	float GetAttributeCurrentValue(FGameplayTag AttributeTag) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Action|Agent|Attributes")
	bool ModifyAttributeCurrentValue(FGameplayTag AttributeTag, float Delta, AActor* InstigatorActor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Action|Agent")
	bool AddStatusTag(const FGameplayTag& StatusTag, int32 Turns, float Seconds, AActor* InstigatorActor);
};
