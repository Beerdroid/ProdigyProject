#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "QuestKillEventSource.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnQuestKillTagNative, FGameplayTag /*TargetTag*/);

UINTERFACE(BlueprintType)
class UQuestKillEventSource : public UInterface
{
	GENERATED_BODY()
};

class IQuestKillEventSource
{
	GENERATED_BODY()

public:
	virtual FOnQuestKillTagNative& GetKillDelegate() = 0;
};
