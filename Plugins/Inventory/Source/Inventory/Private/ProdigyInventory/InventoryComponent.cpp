// InventoryComponent.cpp
#include "ProdigyInventory/InventoryComponent.h"
#include "ProdigyInventory/ItemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "ProdigyInventory/InventoryItemDBProvider.h"

DEFINE_LOG_CATEGORY_STATIC(LogInvPickupCore, Log, All);


UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UInventoryComponent::GetSlotView(int32 SlotIndex, FInventorySlotView& OutView) const
{
	OutView = FInventorySlotView();
	OutView.SlotIndex = SlotIndex;

	if (!IsValidIndex(SlotIndex)) return false;

	const FInventorySlot& S = Slots[SlotIndex];
	if (S.IsEmpty())
	{
		OutView.bEmpty = true;
		return true;
	}

	OutView.bEmpty = false;
	OutView.ItemID = S.ItemID;
	OutView.Quantity = S.Quantity;

	FItemRow Row;
	if (TryGetItemDef(S.ItemID, Row))
	{
		OutView.DisplayName = Row.DisplayName;
		OutView.Description = Row.Description;
		OutView.Icon        = Row.Icon;
		OutView.Category    = Row.Category;
		OutView.Tags        = Row.Tags;
	}
	return true;
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeSlots();

	if (bInitializeOnBeginPlay && PredefinedItems.Num() > 0)
	{
		TArray<int32> Changed;
		for (const FPredefinedItemEntry& Entry : PredefinedItems)
		{
			if (Entry.ItemID.IsNone() || Entry.Quantity <= 0) continue;

			int32 Remainder = 0;
			TArray<int32> LocalChanged;
			AddItem(Entry.ItemID, Entry.Quantity, Remainder, LocalChanged);

			for (int32 Idx : LocalChanged)
			{
				MarkSlotChanged(Idx, Changed);
			}
		}
		BroadcastSlotsChanged(Changed);
	}
}

void UInventoryComponent::BroadcastChanged()
{
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::BroadcastSlotsChanged(const TArray<int32>& ChangedSlots)
{
	if (ChangedSlots.Num() == 0) return;

	OnSlotsChanged.Broadcast(ChangedSlots);

	// Collect item IDs affected by these slot changes
	TSet<FName> ChangedItemIDs;

	for (int32 SlotIdx : ChangedSlots)
	{
		if (!Slots.IsValidIndex(SlotIdx)) continue;

		// Emit per-slot event
		OnSlotChanged.Broadcast(SlotIdx, Slots[SlotIdx]);

		// Current item in slot (after mutation)
		if (!Slots[SlotIdx].ItemID.IsNone())
		{
			ChangedItemIDs.Add(Slots[SlotIdx].ItemID);
		}
	}

	// Coarse inventory changed
	OnInventoryChanged.Broadcast();

	// NEW: notify per ItemID (quests depend on this)
	for (FName ItemID : ChangedItemIDs)
	{
		OnItemIDChanged.Broadcast(ItemID);
	}
}

void UInventoryComponent::InitializeSlots()
{
	Capacity = FMath::Max(0, Capacity);

	Slots.SetNum(Capacity);

	for (FInventorySlot& S : Slots)
	{
		if (S.ItemID.IsNone() || S.Quantity <= 0)
		{
			S.Clear();
		}
	}
}

bool UInventoryComponent::IsValidIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex);
}

bool UInventoryComponent::TryGetItemDef(FName ItemID, FItemRow& OutRow) const
{
	OutRow = FItemRow();
	if (ItemID.IsNone()) return false;

	UWorld* W = GetWorld();
	if (!W) return false;

	AGameStateBase* GS = W->GetGameState();
	if (!GS) return false;

	if (!GS->GetClass()->ImplementsInterface(UInventoryItemDBProvider::StaticClass()))
	{
		return false;
	}

	return IInventoryItemDBProvider::Execute_GetItemRowByID(GS, ItemID, OutRow);
}

FInventorySlot UInventoryComponent::GetSlot(int32 SlotIndex) const
{
	return IsValidIndex(SlotIndex) ? Slots[SlotIndex] : FInventorySlot();
}

bool UInventoryComponent::SetSlot(int32 SlotIndex, const FInventorySlot& NewValue)
{
	if (!IsValidIndex(SlotIndex)) return false;

	Slots[SlotIndex] = NewValue;
	if (Slots[SlotIndex].ItemID.IsNone() || Slots[SlotIndex].Quantity <= 0)
	{
		Slots[SlotIndex].Clear();
	}

	TArray<int32> Changed;
	MarkSlotChanged(SlotIndex, Changed);
	BroadcastSlotsChanged(Changed);
	return true;
}

int32 UInventoryComponent::FindFirstEmptySlot() const
{
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].IsEmpty())
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void UInventoryComponent::FindPartialStacks(FName ItemID, TArray<int32>& OutIndices) const
{
	OutIndices.Reset();
	if (ItemID.IsNone()) return;

	const int32 MaxStack = GetMaxStack(ItemID);
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FInventorySlot& S = Slots[i];
		if (S.ItemID == ItemID && S.Quantity > 0 && S.Quantity < MaxStack)
		{
			OutIndices.Add(i);
		}
	}
}

int32 UInventoryComponent::GetMaxStack(FName ItemID) const
{
	FItemRow Item;
	TryGetItemDef(ItemID, Item);
	return Item.MaxStack;
}

bool UInventoryComponent::IsStackable(FName ItemID) const
{
	FItemRow Item;
	TryGetItemDef(ItemID, Item);
	return Item.bStackable;
}

void UInventoryComponent::MarkSlotChanged(int32 SlotIndex, TArray<int32>& InOutChangedSlots)
{
	if (SlotIndex == INDEX_NONE) return;
	InOutChangedSlots.AddUnique(SlotIndex);
}

int32 UInventoryComponent::GetEmptySlotCount() const
{
	int32 Count = 0;
	for (const FInventorySlot& S : Slots)
	{
		if (S.IsEmpty()) ++Count;
	}
	return Count;
}

bool UInventoryComponent::AddItem(FName ItemID, int32 Quantity, int32& OutRemainder, TArray<int32>& OutChangedSlots)
{
	
	OutChangedSlots.Reset();
	OutRemainder = Quantity;

	if (ItemID.IsNone() || Quantity <= 0) return false;

	FItemRow Item;
	if (!TryGetItemDef(ItemID, Item))
	{
		return false;
	}

	const bool bStackable = Item.bStackable;
	const int32 MaxStack  = FMath::Max(1, Item.MaxStack);

	int32 Remaining = Quantity;

	UE_LOG(LogInvPickupCore, Warning,
		TEXT("[INV][AddItem] Enter Owner=%s ItemID=%s Qty=%d Cap=%d Slots=%d Empty=%d"),
		*GetNameSafe(GetOwner()),
		*ItemID.ToString(),
		Quantity,
		Capacity,
		Slots.Num(),
		GetEmptySlotCount() // implement below
	);

	// 1) fill partial stacks
	if (bStackable)
	{
		TArray<int32> Partial;
		FindPartialStacks(ItemID, Partial);

		for (int32 Idx : Partial)
		{
			if (Remaining <= 0) break;

			FInventorySlot& S = Slots[Idx];
			const int32 Space = FMath::Max(0, MaxStack - S.Quantity);
			const int32 ToAdd = FMath::Min(Space, Remaining);

			if (ToAdd > 0)
			{
				S.Quantity += ToAdd;
				Remaining -= ToAdd;
				MarkSlotChanged(Idx, OutChangedSlots);
			}
		}
	}

	// 2) place into empty slots
	while (Remaining > 0)
	{
		const int32 Empty = FindFirstEmptySlot();
		if (Empty == INDEX_NONE) break;

		FInventorySlot& S = Slots[Empty];
		S.ItemID = ItemID;

		if (bStackable)
		{
			const int32 ToAdd = FMath::Min(MaxStack, Remaining);
			S.Quantity = ToAdd;
			Remaining -= ToAdd;
		}
		else
		{
			S.Quantity = 1;
			Remaining -= 1;
		}

		MarkSlotChanged(Empty, OutChangedSlots);
	}

	OutRemainder = Remaining;

	if (OutChangedSlots.Num() > 0)
	{
		BroadcastSlotsChanged(OutChangedSlots);

		// Optional (if you added Tier-1 quest hook):
		// OnItemIDChanged.Broadcast(ItemID);
	}

	return (Remaining != Quantity);
}

bool UInventoryComponent::RemoveFromSlot(int32 SlotIndex, int32 Quantity, TArray<int32>& OutChangedSlots)
{
	OutChangedSlots.Reset();

	if (!IsValidIndex(SlotIndex)) return false;
	if (Quantity <= 0) return false;

	FInventorySlot& S = Slots[SlotIndex];
	if (S.IsEmpty()) return false;

	const int32 Removed = FMath::Min(Quantity, S.Quantity);
	S.Quantity -= Removed;

	if (S.Quantity <= 0)
	{
		S.Clear();
	}

	MarkSlotChanged(SlotIndex, OutChangedSlots);
	BroadcastSlotsChanged(OutChangedSlots);
	return true;
}

bool UInventoryComponent::MoveOrSwap(int32 FromIndex, int32 ToIndex, TArray<int32>& OutChangedSlots)
{
	OutChangedSlots.Reset();

	if (!IsValidIndex(FromIndex) || !IsValidIndex(ToIndex)) return false;
	if (FromIndex == ToIndex) return false;

	FInventorySlot& From = Slots[FromIndex];
	FInventorySlot& To = Slots[ToIndex];

	if (From.IsEmpty()) return false;

	// Move into empty
	if (To.IsEmpty())
	{
		To = From;
		From.Clear();

		MarkSlotChanged(FromIndex, OutChangedSlots);
		MarkSlotChanged(ToIndex, OutChangedSlots);
		BroadcastSlotsChanged(OutChangedSlots);
		return true;
	}

	// Merge if same ItemID and stackable
	if (From.ItemID == To.ItemID && IsStackable(From.ItemID))
	{
		const int32 MaxStack = GetMaxStack(From.ItemID);
		const int32 Space = FMath::Max(0, MaxStack - To.Quantity);
		if (Space > 0)
		{
			const int32 ToMove = FMath::Min(Space, From.Quantity);
			To.Quantity += ToMove;
			From.Quantity -= ToMove;

			if (From.Quantity <= 0)
			{
				From.Clear();
			}

			MarkSlotChanged(FromIndex, OutChangedSlots);
			MarkSlotChanged(ToIndex, OutChangedSlots);
			BroadcastSlotsChanged(OutChangedSlots);
			return true;
		}
		// If no space, fallthrough to swap like WoW
	}

	// Swap otherwise
	Swap(From, To);
	MarkSlotChanged(FromIndex, OutChangedSlots);
	MarkSlotChanged(ToIndex, OutChangedSlots);
	BroadcastSlotsChanged(OutChangedSlots);
	return true;
}

bool UInventoryComponent::SplitStack(int32 FromIndex, int32 ToIndex, int32 SplitAmount, TArray<int32>& OutChangedSlots)
{
	OutChangedSlots.Reset();

	if (!IsValidIndex(FromIndex) || !IsValidIndex(ToIndex)) return false;
	if (FromIndex == ToIndex) return false;
	if (SplitAmount <= 0) return false;

	FInventorySlot& From = Slots[FromIndex];
	FInventorySlot& To = Slots[ToIndex];

	if (From.IsEmpty()) return false;
	if (!IsStackable(From.ItemID)) return false;
	if (!To.IsEmpty()) return false;

	if (SplitAmount >= From.Quantity) return false;

	To.ItemID = From.ItemID;
	To.Quantity = SplitAmount;

	From.Quantity -= SplitAmount;
	if (From.Quantity <= 0)
	{
		From.Clear();
	}

	MarkSlotChanged(FromIndex, OutChangedSlots);
	MarkSlotChanged(ToIndex, OutChangedSlots);
	BroadcastSlotsChanged(OutChangedSlots);
	return true;
}

bool UInventoryComponent::TransferTo(
	UInventoryComponent* Target,
	int32 FromIndex,
	int32 ToIndex,
	int32 Quantity,
	TArray<int32>& OutChangedSource,
	TArray<int32>& OutChangedTarget)
{
	OutChangedSource.Reset();
	OutChangedTarget.Reset();

	if (!IsValid(Target)) return false;
	if (!IsValidIndex(FromIndex)) return false;
	if (!Target->IsValidIndex(ToIndex)) return false;

	FInventorySlot& From = Slots[FromIndex];
	FInventorySlot& To   = Target->Slots[ToIndex];

	if (From.IsEmpty()) return false;

	const int32 Requested = (Quantity <= 0)
		? From.Quantity
		: FMath::Min(Quantity, From.Quantity);

	// Target empty -> move (possibly partial if stack limits)
	if (To.IsEmpty())
	{
		To.ItemID = From.ItemID;

		if (Target->IsStackable(To.ItemID))
		{
			const int32 MaxStack = Target->GetMaxStack(To.ItemID);
			const int32 ToMove   = FMath::Min(MaxStack, Requested);

			To.Quantity   = ToMove;
			From.Quantity -= ToMove;
		}
		else
		{
			To.Quantity   = 1;
			From.Quantity -= 1;
		}

		if (From.Quantity <= 0) From.Clear();

		MarkSlotChanged(FromIndex, OutChangedSource);
		Target->MarkSlotChanged(ToIndex, OutChangedTarget);

		BroadcastSlotsChanged(OutChangedSource);
		Target->BroadcastSlotsChanged(OutChangedTarget);
		return true;
	}

	// Merge if same item and stackable
	if (To.ItemID == From.ItemID && Target->IsStackable(To.ItemID))
	{
		const int32 MaxStack = Target->GetMaxStack(To.ItemID);
		const int32 Space    = FMath::Max(0, MaxStack - To.Quantity);

		if (Space > 0)
		{
			const int32 ToMove = FMath::Min(Space, Requested);

			To.Quantity   += ToMove;
			From.Quantity -= ToMove;

			if (From.Quantity <= 0) From.Clear();

			MarkSlotChanged(FromIndex, OutChangedSource);
			Target->MarkSlotChanged(ToIndex, OutChangedTarget);

			BroadcastSlotsChanged(OutChangedSource);
			Target->BroadcastSlotsChanged(OutChangedTarget);
			return true;
		}
		// no space -> fallthrough to swap
	}

	// Swap otherwise
	Swap(From, To);

	MarkSlotChanged(FromIndex, OutChangedSource);
	Target->MarkSlotChanged(ToIndex, OutChangedTarget);

	BroadcastSlotsChanged(OutChangedSource);
	Target->BroadcastSlotsChanged(OutChangedTarget);
	return true;
}

bool UInventoryComponent::AutoTransferTo(
	UInventoryComponent* Target,
	int32 FromIndex,
	int32 Quantity,
	TArray<int32>& OutChangedSource,
	TArray<int32>& OutChangedTarget)
{
	OutChangedSource.Reset();
	OutChangedTarget.Reset();

	if (!IsValid(Target)) return false;
	if (!IsValidIndex(FromIndex)) return false;

	FInventorySlot& From = Slots[FromIndex];
	if (From.IsEmpty()) return false;

	int32 Remaining = (Quantity <= 0)
		? From.Quantity
		: FMath::Min(Quantity, From.Quantity);

	const FName ItemID = From.ItemID;

	// 1) Fill partial stacks in target
	if (Target->IsStackable(ItemID))
	{
		const int32 MaxStack = Target->GetMaxStack(ItemID);

		for (int32 i = 0; i < Target->Slots.Num() && Remaining > 0; ++i)
		{
			FInventorySlot& TSlot = Target->Slots[i];

			if (TSlot.ItemID == ItemID && TSlot.Quantity > 0 && TSlot.Quantity < MaxStack)
			{
				const int32 Space  = MaxStack - TSlot.Quantity;
				const int32 ToMove = FMath::Min(Space, Remaining);

				TSlot.Quantity += ToMove;
				Remaining      -= ToMove;

				Target->MarkSlotChanged(i, OutChangedTarget);
			}
		}
	}

	// 2) Fill empty slots in target
	while (Remaining > 0)
	{
		const int32 Empty = Target->FindFirstEmptySlot();
		if (Empty == INDEX_NONE) break;

		FInventorySlot& TSlot = Target->Slots[Empty];
		TSlot.ItemID = ItemID;

		if (Target->IsStackable(ItemID))
		{
			const int32 MaxStack = Target->GetMaxStack(ItemID);
			const int32 ToMove   = FMath::Min(MaxStack, Remaining);

			TSlot.Quantity = ToMove;
			Remaining     -= ToMove;
		}
		else
		{
			TSlot.Quantity = 1;
			Remaining     -= 1;
		}

		Target->MarkSlotChanged(Empty, OutChangedTarget);
	}

	// Remove what we actually moved
	const int32 Requested = (Quantity <= 0)
		? From.Quantity
		: FMath::Min(Quantity, From.Quantity);

	const int32 Moved = Requested - Remaining;
	if (Moved <= 0) return false;

	From.Quantity -= Moved;
	if (From.Quantity <= 0) From.Clear();

	MarkSlotChanged(FromIndex, OutChangedSource);

	BroadcastSlotsChanged(OutChangedSource);
	Target->BroadcastSlotsChanged(OutChangedTarget);
	return true;
}

bool UInventoryComponent::DropFromSlot(int32 SlotIndex, int32 Quantity, FVector WorldLocation, FRotator WorldRotation, TArray<int32>& OutChangedSlots)
{
	OutChangedSlots.Reset();

	if (!IsValidIndex(SlotIndex)) return false;

	FInventorySlot& S = Slots[SlotIndex];
	if (S.IsEmpty()) return false;

	const FName DroppedItemID = S.ItemID; // ✅ capture BEFORE S.Clear / mutation

	// Single access point (works whether you use Subsystem or GS+interface behind it)
	FItemRow Row;
	if (!TryGetItemDef(DroppedItemID, Row)) return false;
	if (Row.bNoDrop) return false;

	const int32 Requested = (Quantity <= 0) ? S.Quantity : FMath::Min(Quantity, S.Quantity);
	if (Requested <= 0) return false;

	// Load pickup class (LoadSynchronous returns null if not set)
	TSubclassOf<AActor> PickupClass = Row.WorldPickupClass.LoadSynchronous();
	if (!PickupClass) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	AActor* Spawned = World->SpawnActor<AActor>(PickupClass, WorldLocation, WorldRotation);
	if (!IsValid(Spawned)) return false;

	// Configure its item component
	UItemComponent* ItemComp = Spawned->FindComponentByClass<UItemComponent>();
	if (!IsValid(ItemComp))
	{
		Spawned->Destroy();
		return false;
	}

	ItemComp->ItemID = DroppedItemID;
	ItemComp->Quantity = Requested;

	// Remove from inventory
	S.Quantity -= Requested;
	if (S.Quantity <= 0)
	{
		S.Clear();
	}

	MarkSlotChanged(SlotIndex, OutChangedSlots);
	BroadcastSlotsChanged(OutChangedSlots);

	OnItemIDChanged.Broadcast(DroppedItemID); // ✅ broadcast the old ID (never None here)

	return true;
}

bool UInventoryComponent::CanFullyAdd(FName ItemID, int32 Quantity) const
{
	if (ItemID.IsNone() || Quantity <= 0) return false;

	const bool bStackable = IsStackable(ItemID);
	const int32 MaxStack = GetMaxStack(ItemID);

	int32 Remaining = Quantity;

	if (bStackable)
	{
		// count free space in existing stacks
		for (const FInventorySlot& S : Slots)
		{
			if (S.ItemID == ItemID && S.Quantity > 0 && S.Quantity < MaxStack)
			{
				Remaining -= (MaxStack - S.Quantity);
				if (Remaining <= 0) return true;
			}
		}
	}

	// count empty slots capacity
	int32 EmptySlots = 0;
	for (const FInventorySlot& S : Slots)
	{
		if (S.IsEmpty()) ++EmptySlots;
	}

	if (!bStackable)
	{
		return EmptySlots >= Remaining;
	}

	// stackable: each empty slot can hold MaxStack
	const int32 TotalSpace = EmptySlots * MaxStack;
	return TotalSpace >= Remaining;
}

bool UInventoryComponent::TryPickupFromItemComponent_FullOnly(UItemComponent* ItemComp)
{
	if (!IsValid(ItemComp))
	{
		UE_LOG(LogInvPickupCore, Warning, TEXT("[INV] TryPickup FAIL: ItemComp invalid"));
		return false;
	}

	AActor* PickupActor = ItemComp->GetOwner();
	if (!IsValid(PickupActor) || PickupActor->IsActorBeingDestroyed())
	{
		UE_LOG(LogInvPickupCore, Warning, TEXT("[INV] TryPickup FAIL: PickupActor invalid/destroying"));
		return false;
	}

	const FName ItemID = ItemComp->ItemID;
	const int32 Qty = ItemComp->Quantity;

	UE_LOG(LogInvPickupCore, Log, TEXT("[INV:%s] TryPickup: ItemOwner=%s ItemID=%s Qty=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(PickupActor),
		*ItemID.ToString(),
		Qty);

	if (ItemID.IsNone() || Qty <= 0)
	{
		UE_LOG(LogInvPickupCore, Warning, TEXT("[INV] TryPickup FAIL: bad ItemID/Qty"));
		return false;
	}

	if (!CanFullyAdd(ItemID, Qty))
	{
		UE_LOG(LogInvPickupCore, Warning, TEXT("[INV] TryPickup BLOCKED: CanFullyAdd=false ItemID=%s Qty=%d Cap=%d Slots=%d"),
			*ItemID.ToString(), Qty, Capacity, Slots.Num());
		return false;
	}

	int32 Remainder = 0;
	TArray<int32> Changed;
	const bool bAddedAny = AddItem(ItemID, Qty, Remainder, Changed);

	UE_LOG(LogInvPickupCore, Log, TEXT("[INV] TryPickup AddItem: AddedAny=%d Remainder=%d Changed=%d"),
		(int32)bAddedAny, Remainder, Changed.Num());

	if (!bAddedAny || Remainder != 0)
	{
		UE_LOG(LogInvPickupCore, Error, TEXT("[INV] TryPickup FAIL: AddItem inconsistent AddedAny=%d Rem=%d"),
			(int32)bAddedAny, Remainder);
		return false;
	}

	UE_LOG(LogInvPickupCore, Log, TEXT("[INV] TryPickup SUCCESS: Destroy %s"), *GetNameSafe(PickupActor));
	PickupActor->Destroy();
	return true;
}

int32 UInventoryComponent::GetTotalQuantityByItemID(FName ItemID) const
{
	if (ItemID.IsNone()) return 0;

	int32 Total = 0;
	for (const FInventorySlot& Slot : Slots)
	{
		if (Slot.ItemID == ItemID && Slot.Quantity > 0)
		{
			Total += Slot.Quantity;
		}
	}
	return Total;
}

bool UInventoryComponent::AddByItemID(FName ItemID, int32 Quantity, int32& OutRemainder, TArray<int32>& OutChangedSlots)
{
	// This is basically just your AddItem wrapper for semantic clarity
	return AddItem(ItemID, Quantity, OutRemainder, OutChangedSlots);
}

bool UInventoryComponent::CanRemoveByItemID(FName ItemID, int32 Quantity) const
{
	if (ItemID.IsNone() || Quantity <= 0) return false;
	return GetTotalQuantityByItemID(ItemID) >= Quantity;
}

bool UInventoryComponent::RemoveByItemID(FName ItemID, int32 Quantity, int32& OutRemoved, TArray<int32>& OutChangedSlots)
{
	OutChangedSlots.Reset();
	OutRemoved = 0;

	if (ItemID.IsNone() || Quantity <= 0) return false;

	int32 Remaining = Quantity;

	// Policy: remove from lowest index first (stable + predictable)
	for (int32 i = 0; i < Slots.Num() && Remaining > 0; ++i)
	{
		FInventorySlot& S = Slots[i];
		if (S.ItemID != ItemID || S.Quantity <= 0) continue;

		const int32 ToRemove = FMath::Min(S.Quantity, Remaining);
		S.Quantity -= ToRemove;
		Remaining -= ToRemove;
		OutRemoved += ToRemove;

		if (S.Quantity <= 0)
		{
			S.Clear();
		}

		MarkSlotChanged(i, OutChangedSlots);
	}

	if (OutChangedSlots.Num() > 0)
	{
		BroadcastSlotsChanged(OutChangedSlots);
	}

	return OutRemoved > 0;
}
