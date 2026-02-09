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

	// --- Turn resource (AP) ---
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Action|Agent")
	bool HasActionPoints(int32 Amount) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Action|Agent")
	bool SpendActionPoints(int32 Amount);

	// --- Combat stats / effects hooks (you can implement however you want) ---
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Action|Agent")
	bool ApplyDamage(float Amount, AActor* InstigatorActor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Action|Agent")
	bool AddStatusTag(const FGameplayTag& StatusTag, int32 Turns, float Seconds, AActor* InstigatorActor);
};
