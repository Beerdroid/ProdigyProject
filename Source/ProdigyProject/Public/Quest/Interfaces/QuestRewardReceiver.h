#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "QuestRewardReceiver.generated.h"

UINTERFACE(BlueprintType)
class UQuestRewardReceiver : public UInterface
{
	GENERATED_BODY()
};

class IQuestRewardReceiver
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Quest|Rewards")
	void AddCurrency(int32 Amount);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Quest|Rewards")
	void AddXP(int32 Amount);
};
