#include "ProdigyInventory/InvDragDropOp.h"

#include "ProdigyInventory/InvPlayerController.h"

void UInvDragDropOp::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

	if (bFromEquipSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DRAG CANCELLED] FromEquipSlot -> no drop-to-world"));
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[DRAG CANCELLED] SrcIdx=%d Handled=%d"),
		SourceIndex,
		bDropHandled ? 1 : 0);

	if (bDropHandled)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DRAG CANCELLED] Skipped (handled by UI)"));
		return;
	}

	if (!IsValid(SourceInventory) || SourceIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DRAG CANCELLED] Invalid source"));
		return;
	}

	// Inventory lives on PC -> use component owner (NOT FirstPlayerController)
	AInvPlayerController* PC = Cast<AInvPlayerController>(SourceInventory->GetOwner());
	if (!IsValid(PC))
	{
		// Fallback (safe in single-player): try world first PC
		UWorld* World = SourceInventory->GetWorld();
		PC = World ? Cast<AInvPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	if (!IsValid(PC))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DRAG CANCELLED] No PC"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[DRAG CANCELLED] DROP TO WORLD"));

	TArray<int32> Changed;
	PC->Player_DropFromSlot(SourceIndex, Quantity, Changed);
}
