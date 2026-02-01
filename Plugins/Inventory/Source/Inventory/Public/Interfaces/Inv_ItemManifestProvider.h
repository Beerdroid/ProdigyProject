#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Items/Manifest/Inv_ItemManifest.h"
#include "Inv_ItemManifestProvider.generated.h"

UINTERFACE(BlueprintType)
class INVENTORY_API UInv_ItemManifestProvider : public UInterface
{
	GENERATED_BODY()
};

class INVENTORY_API IInv_ItemManifestProvider
{
	GENERATED_BODY()

public:
	// BlueprintNativeEvent lets you implement in C++ OR Blueprint.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Inventory|Items")
	bool FindItemManifest(FName ItemID, FInv_ItemManifest& OutManifest) const;
};
