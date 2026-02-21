#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "ProdigyHotbarWidget.h"
#include "ProdigyHotbarDragDropOp.generated.h"

UCLASS()
class PRODIGYPROJECT_API UProdigyHotbarDragDropOp : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<UProdigyHotbarWidget> SourceHotbar;

	UPROPERTY(BlueprintReadOnly)
	int32 SourceIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	FProdigyHotbarEntry DraggedEntry;

	UPROPERTY(BlueprintReadOnly)
	bool bDropHandled = false;

protected:
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override
	{
		Super::DragCancelled_Implementation(PointerEvent);

		UE_LOG(LogTemp, Warning, TEXT("[HotbarDragCancelled] Src=%d Handled=%d Hotbar=%s Type=%d Tag=%s Item=%s"),
			SourceIndex,
			bDropHandled ? 1 : 0,
			*GetNameSafe(SourceHotbar.Get()),
			(int32)DraggedEntry.Type,
			DraggedEntry.AbilityTag.IsValid() ? *DraggedEntry.AbilityTag.ToString() : TEXT("<INVALID>"),
			DraggedEntry.ItemID.IsNone() ? TEXT("<NONE>") : *DraggedEntry.ItemID.ToString());

		// Drop was accepted by some widget => do NOT clear
		if (bDropHandled) return;

		UProdigyHotbarWidget* HB = SourceHotbar.Get();
		if (!IsValid(HB) || SourceIndex == INDEX_NONE) return;

		UE_LOG(LogTemp, Warning, TEXT("[HotbarDragCancelled] Clearing Hotbar Slot=%d"), SourceIndex);
		HB->ClearSlot(SourceIndex);
	}
};