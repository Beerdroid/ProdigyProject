#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Quest/QuestTypes.h"
#include "QuestInventoryProvider.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestInventoryDelta, const FInventoryDelta&, Delta);

UINTERFACE(BlueprintType)
class UQuestInventoryProvider : public UInterface
{
	GENERATED_BODY()
};

class IQuestInventoryProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Quest|Inventory")
	int32 GetTotalQuantityByItemID(FName ItemID) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Quest|Inventory")
	bool AddItemByID(FName ItemID, int32 Quantity, UObject* Context);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Quest|Inventory")
	int32 RemoveItemByID(FName ItemID, int32 Quantity, UObject* Context);

	virtual FOnQuestInventoryDelta& GetInventoryDeltaDelegate() = 0;
};
