#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "InvEquipActor.generated.h"

UCLASS()
class INVENTORY_API AInvEquipActor : public AActor
{
	GENERATED_BODY()

public:
	AInvEquipActor();

	FGameplayTag GetEquipmentType() const { return EquipmentType; }
	void SetEquipmentType(FGameplayTag Type) { EquipmentType = Type; }

	UFUNCTION(BlueprintCallable, Category="Inventory")
	void SetItemID(FName InItemID) { ItemID = InItemID; }

	UFUNCTION(BlueprintPure, Category="Inventory")
	FName GetItemID() const { return ItemID; }
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory")
	FName ItemID = NAME_None;

private:
	UPROPERTY(EditAnywhere, Category="Inventory")
	FGameplayTag EquipmentType;


};
