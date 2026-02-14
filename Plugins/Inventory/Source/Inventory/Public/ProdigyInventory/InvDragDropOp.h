#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/DragDropOperation.h"
#include "InvDragDropOp.generated.h"

class UInventoryComponent;

UCLASS()
class INVENTORY_API UInvDragDropOp : public UDragDropOperation
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UInventoryComponent> SourceInventory = nullptr;

	UPROPERTY(BlueprintReadOnly)
	int32 SourceIndex = INDEX_NONE;

	// Optional: later for split / partial drag
	UPROPERTY(BlueprintReadOnly)
	int32 Quantity = -1; // -1 = "all"

	UPROPERTY(BlueprintReadOnly) bool bDropHandled = false;

	UPROPERTY(BlueprintReadOnly)
	bool bFromEquipSlot = false;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag SourceEquipSlotTag;

protected:
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;
};