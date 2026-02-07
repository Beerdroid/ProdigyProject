// Inv_ItemComponent.h  (for world pickups)
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemComponent.generated.h"

UCLASS(ClassGroup=(Inventory), meta=(BlueprintSpawnableComponent), Blueprintable)
class INVENTORY_API UItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item")
	FName ItemID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item", meta=(ClampMin="1"))
	int32 Quantity = 1;
};
