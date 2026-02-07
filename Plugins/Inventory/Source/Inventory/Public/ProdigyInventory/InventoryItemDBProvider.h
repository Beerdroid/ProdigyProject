#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemTypes.h"
#include "InventoryItemDBProvider.generated.h"

UINTERFACE(BlueprintType)
class INVENTORY_API UInventoryItemDBProvider : public UInterface
{
	GENERATED_BODY()
};

class INVENTORY_API IInventoryItemDBProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|DB")
	bool GetItemRowByID(FName ItemID, FItemRow& OutRow) const;
};
