// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/Inventory/Spatial/Inv_InventoryGrid.h"

#include "Inventory.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Fragments/Inv_FragmentTags.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Widgets/Inventory/GridSlots/Inv_GridSlot.h"
#include "Widgets/Utils/Inv_WidgetUtils.h"
#include "Items/Manifest/Inv_ItemManifest.h"
#include "Player/Inv_PlayerController.h"
#include "Widgets/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widgets/Inventory/SlottedItems/Inv_SlottedItem.h"
#include "Widgets/ItemPopUp/Inv_ItemPopUp.h"

static bool IsPlayerInventoryForThisWidget(const UUserWidget* Widget, const UInv_InventoryComponent* IC)
{
	if (!IsValid(Widget) || !IsValid(IC)) return false;

	APlayerController* PC = Widget->GetOwningPlayer();
	if (!IsValid(PC)) return false;

	AActor* Owner = IC->GetOwner();
	if (!IsValid(Owner)) return false;

	return (Owner == PC) || (Owner == PC->GetPawn());
}

void UInv_InventoryGrid::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ConstructGrid();

	// If someone already set InventoryComponent (external), use it.
	if (InventoryComponent.IsValid())
	{
		bPlayerOwnedInventory = IsPlayerInventoryForThisWidget(this, InventoryComponent.Get()); // ✅
		BindToInventory(InventoryComponent.Get());
		RebuildFromSnapshot();
		return;
	}

	InventoryComponent = UInv_InventoryStatics::GetInventoryComponent(GetOwningPlayer());
	if (!InventoryComponent.IsValid()) return;

	bPlayerOwnedInventory = true; // ✅ since you explicitly grabbed the player's IC
	BindToInventory(InventoryComponent.Get());
	RebuildFromSnapshot();
}

void UInv_InventoryGrid::SetInventoryComponent(UInv_InventoryComponent* InComp)
{
	if (InventoryComponent.Get() == InComp) return;

	UnbindFromInventory();

	InventoryComponent = InComp;
	if (!InventoryComponent.IsValid()) return;

	bPlayerOwnedInventory = IsPlayerInventoryForThisWidget(this, InventoryComponent.Get());

	BindToInventory(InventoryComponent.Get());
	RebuildFromSnapshot();
}


void UInv_InventoryGrid::BindToInventory(UInv_InventoryComponent* InComp)
{
	if (!IsValid(InComp)) return;

	// always remove first to avoid duplicate binds
	InComp->OnItemAdded.RemoveDynamic(this, &ThisClass::AddItem);
	InComp->OnItemAdded.AddDynamic(this, &ThisClass::AddItem);

	InComp->OnStackChange.RemoveDynamic(this, &ThisClass::AddStacks);
	InComp->OnStackChange.AddDynamic(this, &ThisClass::AddStacks);

	InComp->OnInventoryMenuToggled.RemoveDynamic(this, &ThisClass::OnInventoryMenuToggled);
	InComp->OnInventoryMenuToggled.AddDynamic(this, &ThisClass::OnInventoryMenuToggled);

	InComp->OnItemRemoved.RemoveDynamic(this, &ThisClass::HandleItemRemoved);
	InComp->OnItemRemoved.AddDynamic(this, &ThisClass::HandleItemRemoved);

	InComp->OnInvDelta.RemoveAll(this);
	InComp->OnInvDelta.AddUObject(this, &ThisClass::HandleInvDelta);
}

void UInv_InventoryGrid::UnbindFromInventory()
{
	if (!InventoryComponent.IsValid()) return;

	InventoryComponent->OnItemAdded.RemoveDynamic(this, &ThisClass::AddItem);
	InventoryComponent->OnStackChange.RemoveDynamic(this, &ThisClass::AddStacks);
	InventoryComponent->OnInventoryMenuToggled.RemoveDynamic(this, &ThisClass::OnInventoryMenuToggled);
	InventoryComponent->OnItemRemoved.RemoveDynamic(this, &ThisClass::HandleItemRemoved);
	InventoryComponent->OnInvDelta.RemoveAll(this);
}

void UInv_InventoryGrid::RebuildFromSnapshot()
{
	if (!bGridBuilt || !InventoryComponent.IsValid())
		return;

	// Clear only visuals we own (slotted widgets), keep GridSlots array
	for (auto& KVP : SlottedItems)
	{
		if (IsValid(KVP.Value))
		{
			KVP.Value->RemoveFromParent();
		}
	}
	SlottedItems.Empty();

	// Reset slot state (NO popup touching here)
	for (UInv_GridSlot* GridSlot : GridSlots)
	{
		if (!IsValid(GridSlot)) continue;

		GridSlot->SetInventoryItem(nullptr);
		GridSlot->SetUpperLeftIndex(INDEX_NONE);
		GridSlot->SetAvailable(true);
		GridSlot->SetStackCount(0);
		GridSlot->SetUnoccupiedTexture();
	}

	// Now rebuild from the inventory's current state
	const TArray<UInv_InventoryItem*> Items = InventoryComponent->GetAllItems(); // or whatever you have
	for (UInv_InventoryItem* Item : Items)
	{
		if (!IsValid(Item)) continue;
		AddItem(Item);
	}

	bSnapshotApplied = true;
}

void UInv_InventoryGrid::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const FVector2D CanvasPosition = UInv_WidgetUtils::GetWidgetPosition(CanvasPanel);
	const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());

	if (CursorExitedCanvas(CanvasPosition, UInv_WidgetUtils::GetWidgetSize(CanvasPanel), MousePosition))
	{
		return;
	}

	UpdateTileParameters(CanvasPosition, MousePosition);
}

FInv_SlotAvailabilityResult UInv_InventoryGrid::HasRoomForItem(FName ItemID, const UInv_ItemComponent* ItemComponent)
{
	return HasRoomForItem(ItemID, ItemComponent->GetItemManifest());
}

FInv_SlotAvailabilityResult UInv_InventoryGrid::HasRoomForItem(FName ItemID, const FInv_ItemManifest& Manifest,
                                                               const int32 StackAmountOverride)
{
	FInv_SlotAvailabilityResult Result;

	const FInv_StackableFragment* StackableFragment = Manifest.GetFragmentOfType<FInv_StackableFragment>();
	Result.bStackable = StackableFragment != nullptr;

	const int32 MaxStackSize = StackableFragment ? StackableFragment->GetMaxStackSize() : 1;
	int32 AmountToFill = StackableFragment ? StackableFragment->GetStackCount() : 1;

	if (StackAmountOverride != -1 && Result.bStackable)
	{
		AmountToFill = StackAmountOverride;
	}

	TSet<int32> CheckedIndices;

	for (const auto& GridSlot : GridSlots)
	{
		if (AmountToFill == 0) break;
		if (IsIndexClaimed(CheckedIndices, GridSlot->GetIndex())) continue;
		if (!IsInGridBounds(GridSlot->GetIndex(), GetItemDimensions(Manifest))) continue;

		TSet<int32> TentativelyClaimed;

		if (!HasRoomAtIndex(GridSlot, GetItemDimensions(Manifest), CheckedIndices, TentativelyClaimed, ItemID,
		                    MaxStackSize))
		{
			continue;
		}

		const int32 AmountToFillInSlot = DetermineFillAmountForSlot(Result.bStackable, MaxStackSize, AmountToFill,
		                                                            GridSlot);
		if (AmountToFillInSlot == 0) continue;

		CheckedIndices.Append(TentativelyClaimed);

		Result.TotalRoomToFill += AmountToFillInSlot;
		Result.SlotAvailabilities.Emplace(FInv_SlotAvailability{
			HasValidItem(GridSlot) ? GridSlot->GetUpperLeftIndex() : GridSlot->GetIndex(),
			Result.bStackable ? AmountToFillInSlot : 0,
			HasValidItem(GridSlot)
		});

		AmountToFill -= AmountToFillInSlot;
		Result.Remainder = AmountToFill;

		if (AmountToFill == 0) return Result;
	}

	return Result;
}


FInv_SlotAvailabilityResult UInv_InventoryGrid::HasRoomForItem(FName ItemID, const UInv_InventoryItem* Item,
                                                               const int32 StackAmountOverride)
{
	return HasRoomForItem(ItemID, Item->GetItemManifest(), StackAmountOverride);
}

FInv_SlotAvailabilityResult UInv_InventoryGrid::HasRoomForItem(const UInv_ItemComponent* ItemComponent)
{
	FInv_SlotAvailabilityResult Result;
	if (!IsValid(ItemComponent))
	{
		return Result;
	}

	const FName ItemID = ItemComponent->GetItemID();
	return HasRoomForItem(ItemID, ItemComponent->GetItemManifest(), /*StackAmountOverride*/ -1);
}

FInv_SlotAvailabilityResult UInv_InventoryGrid::HasRoomForItem(const UInv_InventoryItem* Item,
                                                               int32 StackAmountOverride)
{
	FInv_SlotAvailabilityResult Result;
	if (!IsValid(Item))
	{
		return Result;
	}

	const FName ItemID = Item->GetItemID();
	return HasRoomForItem(ItemID, Item, StackAmountOverride);
}

void UInv_InventoryGrid::UpdateTileParameters(const FVector2D& CanvasPosition, const FVector2D& MousePosition)
{
	if (!bMouseWithinCanvas) return;

	// Calculate the tile quadrant, tile index, and coordinates
	const FIntPoint HoveredTileCoordinates = CalculateHoveredCoordinates(CanvasPosition, MousePosition);

	LastTileParameters = TileParameters;
	TileParameters.TileCoordinats = HoveredTileCoordinates;
	TileParameters.TileIndex = UInv_WidgetUtils::GetIndexFromPosition(HoveredTileCoordinates, Columns);
	TileParameters.TileQuadrant = CalculateTileQuadrant(CanvasPosition, MousePosition);

	OnTileParametersUpdated(TileParameters);
}

void UInv_InventoryGrid::HandleInvDelta(FName ItemID, int32 DeltaQty, UObject* Context)
{
	if (ItemID.IsNone() || DeltaQty == 0) return;

	// Only care about decreases here (quest turn-in, consume, etc.)
	if (DeltaQty > 0) return;

	// Find the item in THIS grid by ItemID
	int32 FoundUpperLeft = INDEX_NONE;
	UInv_InventoryItem* FoundItem = nullptr;

	for (int32 i = 0; i < GridSlots.Num(); ++i)
	{
		UInv_GridSlot* GridSlot = GridSlots[i];
		if (!IsValid(GridSlot)) continue;

		UInv_InventoryItem* SlotItem = GridSlot->GetInventoryItem().Get();
		if (!IsValid(SlotItem)) continue;
		if (!MatchesCategory(SlotItem)) continue;

		if (SlotItem->GetItemID() == ItemID)
		{
			FoundItem = SlotItem;
			FoundUpperLeft = (GridSlot->GetUpperLeftIndex() != INDEX_NONE) ? GridSlot->GetUpperLeftIndex() : i;
			break;
		}
	}

	if (!IsValid(FoundItem) || !GridSlots.IsValidIndex(FoundUpperLeft)) return;

	// Authoritative new count from inventory
	int32 NewTotal = 0;
	if (InventoryComponent.IsValid())
	{
		NewTotal = InventoryComponent.Get()->GetTotalQuantityByItemID(ItemID);
	}
	// else: no IC -> can't resolve totals safely, just bail
	else
	{
		return;
	}

	// If item is gone -> clear UI (same as consume/drop when stack hits 0)
	if (NewTotal <= 0)
	{
		RemoveItemFromGrid(FoundItem, FoundUpperLeft);
		return;
	}

	// Otherwise update stack count UI for upper-left slot & widget
	GridSlots[FoundUpperLeft]->SetStackCount(NewTotal);

	if (TObjectPtr<UInv_SlottedItem>* Slotted = SlottedItems.Find(FoundUpperLeft))
	{
		if (IsValid(*Slotted))
		{
			(*Slotted)->UpdateStackCount(NewTotal);
		}
	}
}

void UInv_InventoryGrid::HandleItemRemoved(UInv_InventoryItem* Item)
{
	if (!IsValid(Item)) return;
	if (!MatchesCategory(Item)) return;

	// Find ANY slot that references this item, then remove by upper-left index
	for (int32 i = 0; i < GridSlots.Num(); ++i)
	{
		if (!IsValid(GridSlots[i])) continue;

		if (GridSlots[i]->GetInventoryItem().Get() == Item)
		{
			const int32 UpperLeft = GridSlots[i]->GetUpperLeftIndex();
			const int32 RemoveIndex = (UpperLeft != INDEX_NONE) ? UpperLeft : i;

			RemoveItemFromGrid(Item, RemoveIndex);
			return;
		}
	}
}

void UInv_InventoryGrid::OnTileParametersUpdated(const FInv_TileParameters& Parameters)
{
	if (!IsValid(HoverItem)) return;

	// Get Hover Item's dimensions
	const FIntPoint Dimensions = HoverItem->GetGridDimensions();

	// calculate the starting coordinate for highlighting
	const FIntPoint StartingCoordinate = CalculateStartingCoordinate(Parameters.TileCoordinats, Dimensions,
	                                                                 Parameters.TileQuadrant);
	ItemDropIndex = UInv_WidgetUtils::GetIndexFromPosition(StartingCoordinate, Columns);

	CurrentQueryResult = CheckHoverPosition(StartingCoordinate, Dimensions);

	if (CurrentQueryResult.bHasSpace)
	{
		HighlightSlots(ItemDropIndex, Dimensions);
		return;
	}
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);

	if (CurrentQueryResult.ValidItem.IsValid() && GridSlots.IsValidIndex(CurrentQueryResult.UpperLeftIndex))
	{
		const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(
			CurrentQueryResult.ValidItem.Get(), FragmentTags::GridFragment);
		if (!GridFragment) return;

		ChangeHoverType(CurrentQueryResult.UpperLeftIndex, GridFragment->GetGridSize(), EInv_GridSlotState::GrayedOut);
	}
}

FInv_SpaceQueryResult UInv_InventoryGrid::CheckHoverPosition(const FIntPoint& Position, const FIntPoint& Dimensions)
{
	FInv_SpaceQueryResult Result;

	// in the grid bounds?
	if (!IsInGridBounds(UInv_WidgetUtils::GetIndexFromPosition(Position, Columns), Dimensions)) return Result;

	Result.bHasSpace = true;

	// If more than one of the indices is occupied with the same item, we need to see if they all have the same upper left index.
	TSet<int32> OccupiedUpperLeftIndices;
	UInv_InventoryStatics::ForEach2D(GridSlots, UInv_WidgetUtils::GetIndexFromPosition(Position, Columns), Dimensions,
	                                 Columns, [&](const UInv_GridSlot* GridSlot)
	                                 {
		                                 if (GridSlot->GetInventoryItem().IsValid())
		                                 {
			                                 OccupiedUpperLeftIndices.Add(GridSlot->GetUpperLeftIndex());
			                                 Result.bHasSpace = false;
		                                 }
	                                 });

	// if so, is there only one item in the way? (can we swap?)
	if (OccupiedUpperLeftIndices.Num() == 1) // single item at position - it's valid for swapping/combining
	{
		const int32 Index = *OccupiedUpperLeftIndices.CreateConstIterator();
		Result.ValidItem = GridSlots[Index]->GetInventoryItem();
		Result.UpperLeftIndex = GridSlots[Index]->GetUpperLeftIndex();
	}

	return Result;
}

bool UInv_InventoryGrid::CursorExitedCanvas(const FVector2D& BoundaryPos, const FVector2D& BoundarySize,
                                            const FVector2D& Location)
{
	bLastMouseWithinCanvas = bMouseWithinCanvas;
	bMouseWithinCanvas = UInv_WidgetUtils::IsWithinBounds(BoundaryPos, BoundarySize, Location);
	if (!bMouseWithinCanvas && bLastMouseWithinCanvas)
	{
		UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
		return true;
	}
	return false;
}

void UInv_InventoryGrid::HighlightSlots(const int32 Index, const FIntPoint& Dimensions)
{
	if (!bMouseWithinCanvas) return;
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	UInv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, Columns, [&](UInv_GridSlot* GridSlot)
	{
		GridSlot->SetOccupiedTexture();
	});
	LastHighlightedDimensions = Dimensions;
	LastHighlightedIndex = Index;
}

void UInv_InventoryGrid::UnHighlightSlots(const int32 Index, const FIntPoint& Dimensions)
{
	UInv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, Columns, [&](UInv_GridSlot* GridSlot)
	{
		if (GridSlot->IsAvailable())
		{
			GridSlot->SetUnoccupiedTexture();
		}
		else
		{
			GridSlot->SetOccupiedTexture();
		}
	});
}

void UInv_InventoryGrid::ChangeHoverType(const int32 Index, const FIntPoint& Dimensions,
                                         EInv_GridSlotState GridSlotState)
{
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	UInv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, Columns,
	                                 [State = GridSlotState](UInv_GridSlot* GridSlot)
	                                 {
		                                 switch (State)
		                                 {
		                                 case EInv_GridSlotState::Occupied:
			                                 GridSlot->SetOccupiedTexture();
			                                 break;
		                                 case EInv_GridSlotState::Unoccupied:
			                                 GridSlot->SetUnoccupiedTexture();
			                                 break;
		                                 case EInv_GridSlotState::GrayedOut:
			                                 GridSlot->SetGrayedOutTexture();
			                                 break;
		                                 case EInv_GridSlotState::Selected:
			                                 GridSlot->SetSelectedTexture();
			                                 break;
		                                 }
	                                 });

	LastHighlightedIndex = Index;
	LastHighlightedDimensions = Dimensions;
}

FIntPoint UInv_InventoryGrid::CalculateStartingCoordinate(const FIntPoint& Coordinate, const FIntPoint& Dimensions,
                                                          const EInv_TileQuadrant Quadrant) const
{
	const int32 HasEvenWidth = Dimensions.X % 2 == 0 ? 1 : 0;
	const int32 HasEvenHeight = Dimensions.Y % 2 == 0 ? 1 : 0;

	FIntPoint StartingCoord;
	switch (Quadrant)
	{
	case EInv_TileQuadrant::TopLeft:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X);
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y);
		break;
	case EInv_TileQuadrant::TopRight:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X) + HasEvenWidth;
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y);
		break;
	case EInv_TileQuadrant::BottomLeft:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X);
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y) + HasEvenHeight;
		break;
	case EInv_TileQuadrant::BottomRight:
		StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5f * Dimensions.X) + HasEvenWidth;
		StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5f * Dimensions.Y) + HasEvenHeight;
		break;
	default:
		UE_LOG(LogInventory, Error, TEXT("Invalid Quadrant."))
		return FIntPoint(-1, -1);
	}
	return StartingCoord;
}

FIntPoint UInv_InventoryGrid::CalculateHoveredCoordinates(const FVector2D& CanvasPosition,
                                                          const FVector2D& MousePosition) const
{
	return FIntPoint{
		static_cast<int32>(FMath::FloorToInt((MousePosition.X - CanvasPosition.X) / TileSize)),
		static_cast<int32>(FMath::FloorToInt((MousePosition.Y - CanvasPosition.Y) / TileSize))
	};
}

EInv_TileQuadrant UInv_InventoryGrid::CalculateTileQuadrant(const FVector2D& CanvasPosition,
                                                            const FVector2D& MousePosition) const
{
	// Calculate relative position within the current tile
	const float TileLocalX = FMath::Fmod(MousePosition.X - CanvasPosition.X, TileSize);
	const float TileLocalY = FMath::Fmod(MousePosition.Y - CanvasPosition.Y, TileSize);

	// Determine which quadrant the mouse is in
	const bool bIsTop = TileLocalY < TileSize / 2.f; // Top if Y is in the upper half
	const bool bIsLeft = TileLocalX < TileSize / 2.f; // Left if X is in the left half

	EInv_TileQuadrant HoveredTileQuadrant{EInv_TileQuadrant::None};
	if (bIsTop && bIsLeft) HoveredTileQuadrant = EInv_TileQuadrant::TopLeft;
	else if (bIsTop && !bIsLeft) HoveredTileQuadrant = EInv_TileQuadrant::TopRight;
	else if (!bIsTop && bIsLeft) HoveredTileQuadrant = EInv_TileQuadrant::BottomLeft;
	else if (!bIsTop && !bIsLeft) HoveredTileQuadrant = EInv_TileQuadrant::BottomRight;

	return HoveredTileQuadrant;
}


bool UInv_InventoryGrid::HasRoomAtIndex(const UInv_GridSlot* GridSlot, const FIntPoint& Dimensions,
                                        const TSet<int32>& CheckedIndices, TSet<int32>& OutTentativelyClaimed,
                                        const FName ItemID, const int32 MaxStackSize)
{
	// Is there room at this index? (i.e. are there other items in the way?)
	bool bHasRoomAtIndex = true;
	UInv_InventoryStatics::ForEach2D(GridSlots, GridSlot->GetIndex(), Dimensions, Columns,
	                                 [&](const UInv_GridSlot* SubGridSlot)
	                                 {
		                                 if (CheckSlotConstraints(GridSlot, SubGridSlot, CheckedIndices,
		                                                          OutTentativelyClaimed, ItemID, MaxStackSize))
		                                 {
			                                 OutTentativelyClaimed.Add(SubGridSlot->GetIndex());
		                                 }
		                                 else
		                                 {
			                                 bHasRoomAtIndex = false;
		                                 }
	                                 });

	return bHasRoomAtIndex;
}


bool UInv_InventoryGrid::CheckSlotConstraints(const UInv_GridSlot* GridSlot, const UInv_GridSlot* SubGridSlot,
                                              const TSet<int32>& CheckedIndices, TSet<int32>& OutTentativelyClaimed,
                                              const FName ItemID,
                                              const int32 MaxStackSize) const
{
	if (IsIndexClaimed(CheckedIndices, SubGridSlot->GetIndex())) return false;

	if (!HasValidItem(SubGridSlot))
	{
		OutTentativelyClaimed.Add(SubGridSlot->GetIndex());
		return true;
	}

	if (!IsUpperLeftSlot(GridSlot, SubGridSlot)) return false;

	const UInv_InventoryItem* SubItem = SubGridSlot->GetInventoryItem().Get();
	if (!IsValid(SubItem) || !SubItem->IsStackable()) return false;

	// ID match (not type match)
	const FName SubItemID = SubItem->GetItemID();
	if (SubItemID.IsNone() || SubItemID != ItemID) return false;

	if (GridSlot->GetStackCount() >= MaxStackSize) return false;

	return true;
}

FIntPoint UInv_InventoryGrid::GetItemDimensions(const FInv_ItemManifest& Manifest) const
{
	const FInv_GridFragment* GridFragment = Manifest.GetFragmentOfType<FInv_GridFragment>();
	return GridFragment ? GridFragment->GetGridSize() : FIntPoint(1, 1);
}

bool UInv_InventoryGrid::HasValidItem(const UInv_GridSlot* GridSlot) const
{
	return GridSlot->GetInventoryItem().IsValid();
}

bool UInv_InventoryGrid::IsUpperLeftSlot(const UInv_GridSlot* GridSlot, const UInv_GridSlot* SubGridSlot) const
{
	return SubGridSlot->GetUpperLeftIndex() == GridSlot->GetIndex();
}

bool UInv_InventoryGrid::DoesItemTypeMatch(const UInv_InventoryItem* SubItem, FName ItemID) const
{
	return IsValid(SubItem) && !ItemID.IsNone() && SubItem->GetItemID() == ItemID;
}

bool UInv_InventoryGrid::IsInGridBounds(const int32 StartIndex, const FIntPoint& ItemDimensions) const
{
	if (StartIndex < 0 || StartIndex >= GridSlots.Num()) return false;
	const int32 EndColumn = (StartIndex % Columns) + ItemDimensions.X;
	const int32 EndRow = (StartIndex / Columns) + ItemDimensions.Y;
	return EndColumn <= Columns && EndRow <= Rows;
}

int32 UInv_InventoryGrid::DetermineFillAmountForSlot(const bool bStackable, const int32 MaxStackSize,
                                                     const int32 AmountToFill, const UInv_GridSlot* GridSlot) const
{
	const int32 RoomInSlot = MaxStackSize - GetStackAmount(GridSlot);
	return bStackable ? FMath::Min(AmountToFill, RoomInSlot) : 1;
}

int32 UInv_InventoryGrid::GetStackAmount(const UInv_GridSlot* GridSlot) const
{
	int32 CurrentSlotStackCount = GridSlot->GetStackCount();
	// If we are at a slot that doesn't hold the stack count. we must get the actual stack count.
	if (const int32 UpperLeftIndex = GridSlot->GetUpperLeftIndex(); UpperLeftIndex != INDEX_NONE)
	{
		UInv_GridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
		CurrentSlotStackCount = UpperLeftGridSlot->GetStackCount();
	}
	return CurrentSlotStackCount;
}

bool UInv_InventoryGrid::IsRightClick(const FPointerEvent& MouseEvent) const
{
	return MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
}

bool UInv_InventoryGrid::IsLeftClick(const FPointerEvent& MouseEvent) const
{
	return MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
}

bool UInv_InventoryGrid::IsMiddleClick(const FPointerEvent& MouseEvent) const
{
	return MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton;
}

void UInv_InventoryGrid::PickUp(UInv_InventoryItem* ClickedInventoryItem, const int32 GridIndex)
{
	AssignHoverItem(ClickedInventoryItem, GridIndex, GridIndex);
	RemoveItemFromGrid(ClickedInventoryItem, GridIndex);
}

void UInv_InventoryGrid::AssignHoverItem(UInv_InventoryItem* InventoryItem, const int32 GridIndex,
                                         const int32 PreviousGridIndex)
{
	AssignHoverItem(InventoryItem);

	HoverItem->SetPreviousGridIndex(PreviousGridIndex);
	HoverItem->UpdateStackCount(InventoryItem->IsStackable() ? GridSlots[GridIndex]->GetStackCount() : 0);
}

void UInv_InventoryGrid::RemoveItemFromGrid(UInv_InventoryItem* InventoryItem, const int32 GridIndex)
{
	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(InventoryItem, FragmentTags::GridFragment);
	if (!GridFragment) return;

	UInv_InventoryStatics::ForEach2D(GridSlots, GridIndex, GridFragment->GetGridSize(), Columns,
	                                 [&](UInv_GridSlot* GridSlot)
	                                 {
		                                 GridSlot->SetInventoryItem(nullptr);
		                                 GridSlot->SetUpperLeftIndex(INDEX_NONE);
		                                 GridSlot->SetUnoccupiedTexture();
		                                 GridSlot->SetAvailable(true);
		                                 GridSlot->SetStackCount(0);
	                                 });

	if (SlottedItems.Contains(GridIndex))
	{
		TObjectPtr<UInv_SlottedItem> FoundSlottedItem;
		SlottedItems.RemoveAndCopyValue(GridIndex, FoundSlottedItem);
		FoundSlottedItem->RemoveFromParent();
	}
}

void UInv_InventoryGrid::AssignHoverItem(UInv_InventoryItem* InventoryItem)
{
	if (!IsValid(HoverItem))
	{
		HoverItem = CreateWidget<UInv_HoverItem>(GetOwningPlayer(), HoverItemClass);
	}

	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(InventoryItem, FragmentTags::GridFragment);
	const FInv_ImageFragment* ImageFragment = GetFragment<
		FInv_ImageFragment>(InventoryItem, FragmentTags::IconFragment);
	if (!GridFragment || !ImageFragment) return;

	const FVector2D DrawSize = GetDrawSize(GridFragment);

	FSlateBrush IconBrush;
	IconBrush.SetResourceObject(ImageFragment->GetIcon());
	IconBrush.DrawAs = ESlateBrushDrawType::Image;
	IconBrush.ImageSize = DrawSize * UWidgetLayoutLibrary::GetViewportScale(this);

	HoverItem->SetImageBrush(IconBrush);
	HoverItem->SetGridDimensions(GridFragment->GetGridSize());
	HoverItem->SetInventoryItem(InventoryItem);
	HoverItem->SetIsStackable(InventoryItem->IsStackable());

	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, HoverItem);
}

void UInv_InventoryGrid::OnHide()
{
	PutHoverItemBack();
}

void UInv_InventoryGrid::AddStacks(const FInv_SlotAvailabilityResult& Result)
{
	if (!MatchesCategory(Result.Item.Get())) return;

	for (const auto& Availability : Result.SlotAvailabilities)
	{
		if (Availability.bItemAtIndex)
		{
			const auto& GridSlot = GridSlots[Availability.Index];
			const auto& SlottedItem = SlottedItems.FindChecked(Availability.Index);
			SlottedItem->UpdateStackCount(GridSlot->GetStackCount() + Availability.AmountToFill);
			GridSlot->SetStackCount(GridSlot->GetStackCount() + Availability.AmountToFill);
		}
		else
		{
			AddItemAtIndex(Result.Item.Get(), Availability.Index, Result.bStackable, Availability.AmountToFill);
			UpdateGridSlots(Result.Item.Get(), Availability.Index, Result.bStackable, Availability.AmountToFill);
		}
	}
}

void UInv_InventoryGrid::OnSlottedItemClicked(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	// UI code must never crash the game.
	if (!GridSlots.IsValidIndex(GridIndex) || !IsValid(GridSlots[GridIndex]))
	{
		return;
	}

	if (!bPlayerOwnedInventory && IsLeftClick(MouseEvent))
	{
		UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
		return;
	}

	if (IsRightClick(MouseEvent) && !bPlayerOwnedInventory)
	{
		UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
		return;
	}

	// Snapshot pointers BEFORE any unhover/cleanup, because unhover can invalidate hover state.
	const bool bLeftClick = IsLeftClick(MouseEvent);
	const bool bRightClick = IsRightClick(MouseEvent);
	const bool bMiddleClick = IsMiddleClick(MouseEvent);

	UInv_InventoryItem* ClickedInventoryItem = GridSlots[GridIndex]->GetInventoryItem().Get();
	const bool bClickedItemValid = IsValid(ClickedInventoryItem);

	// Optional: snapshot hover item pointers too (do not trust HoverItem after unhover)
	const bool bHasHoverWidget = IsValid(HoverItem);
	UInv_InventoryItem* HoverInvItem = bHasHoverWidget ? HoverItem->GetInventoryItem() : nullptr;
	const bool bHoverInvItemValid = IsValid(HoverInvItem);

	UE_LOG(LogTemp, Warning, TEXT("[INV][GridClick] idx=%d L=%d R=%d M=%d PlayerOwned=%d UIMode=%d"),
	       GridIndex, bLeftClick, bRightClick, bMiddleClick, bPlayerOwnedInventory, (int32)UIMode);

	// MIDDLE CLICK = Sell (only from player inventory, only if merchant UI context)
	if (bMiddleClick)
	{
		UE_LOG(LogTemp, Warning, TEXT("[INV][Sell] MMB detected idx=%d"), GridIndex);

		// Sell only from player inventory while merchant is open
		if (!bPlayerOwnedInventory || UIMode != EInv_GridUIMode::PlayerInventory)
		{
			UE_LOG(LogTemp, Warning, TEXT("[INV][Sell] blocked: not player inventory"));
			UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
			return;
		}

		if (!bClickedItemValid)
		{
			UE_LOG(LogTemp, Warning, TEXT("[INV][Sell] blocked: clicked item invalid"));
			UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
			return;
		}

		AInv_PlayerController* InvPC = Cast<AInv_PlayerController>(GetOwningPlayer());
		if (!InvPC)
		{
			UE_LOG(LogTemp, Warning, TEXT("[INV][Sell] blocked: InvPC cast failed"));
			UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
			return;
		}

		// FIXED CONDITION (you already fixed earlier)
		if (!InventoryComponent.IsValid() || !IsValid(InventoryComponent->GetExternalInventoryComp()))
		{
			UE_LOG(LogTemp, Warning, TEXT("[INV][Sell] blocked: no external inventory (merchant) open"));
			UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
			return;
		}

		UInv_InventoryComponent* Source = InventoryComponent.Get();
		UInv_InventoryComponent* Target = InventoryComponent->GetExternalInventoryComp();

		const FName ItemID = ClickedInventoryItem->GetItemID();
		const int32 Quantity = MouseEvent.IsShiftDown() ? GridSlots[GridIndex]->GetStackCount() : 1;

		UE_LOG(LogTemp, Warning, TEXT("[INV][Sell] RPC -> Server_MoveItem ItemID=%s Qty=%d"),
			*ItemID.ToString(), Quantity);

		InvPC->Server_MoveItem(Source, Target, ItemID, Quantity, EInv_MoveReason::Sell);

		UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
		return;
	}

	// Context menu / popup should typically work even if slot is empty (if you want), but you currently assume item exists.
	if (bRightClick)
	{
		if (UIMode == EInv_GridUIMode::ExternalInventory)
		{
			// No item popup in external inventory
			UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
			return;
		}

		if (bClickedItemValid)
		{
			CreateItemPopUp(GridIndex);
		}

		UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
		return;
	}

	// LEFT CLICK behavior
	if (bLeftClick)
	{
		if (UIMode == EInv_GridUIMode::ExternalInventory)
		{
			// You can handle "buy on left click" here if you want:
			// RequestBuy(ClickedInventoryItem, 1);
			UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
			return;
		}
		// If we're not holding anything (no hover item), clicking a valid item picks it up.
		if (!bHasHoverWidget)
		{
			if (bClickedItemValid)
			{
				PickUp(ClickedInventoryItem, GridIndex);
			}

			UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
			return;
		}

		// If we ARE holding something, clicking an empty slot should place it (if your code supports that).
		// If you don't have placement logic, just bail safely.
		if (!bClickedItemValid)
		{
			// Example if you have it:
			// PlaceHoverItemIntoEmptySlot(GridIndex);
			UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
			return;
		}

		// From here: clicked item valid AND hover widget exists.
		// Stacking logic only if hover inventory item valid.
		if (bHoverInvItemValid && IsSameStackable(ClickedInventoryItem))
		{
			const int32 ClickedStackCount = GridSlots[GridIndex]->GetStackCount();

			const FInv_StackableFragment* ClickedStackFrag =
				ClickedInventoryItem->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();

			// Defensive: if stackable flag says true but fragment is missing, do not crash.
			if (!ClickedStackFrag)
			{
				UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
				return;
			}

			const int32 MaxStackSize = ClickedStackFrag->GetMaxStackSize();
			const int32 RoomInClickedSlot = MaxStackSize - ClickedStackCount;

			const int32 HoveredStackCount = HoverItem->GetStackCount();
			// HoverItem is valid, but stack count should be safe
			// If HoverItem->GetStackCount() internally uses invalid data, you must guard it similarly.

			if (ShouldSwapStackCounts(RoomInClickedSlot, HoveredStackCount, MaxStackSize))
			{
				SwapStackCounts(ClickedStackCount, HoveredStackCount, GridIndex);
				UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
				return;
			}

			if (ShouldConsumeHoverItemStacks(HoveredStackCount, RoomInClickedSlot))
			{
				ConsumeHoverItemStacks(ClickedStackCount, HoveredStackCount, GridIndex);
				UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
				return;
			}

			if (ShouldFillInStack(RoomInClickedSlot, HoveredStackCount))
			{
				FillInStack(RoomInClickedSlot, HoveredStackCount - RoomInClickedSlot, GridIndex);
				UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
				return;
			}

			// Clicked slot already full; do nothing.
			UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
			return;
		}

		// Swap logic: only if your query result says it's valid and we have a clicked item
		if (CurrentQueryResult.ValidItem.IsValid())
		{
			SwapWithHoverItem(ClickedInventoryItem, GridIndex);
		}

		UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
		return;
	}

	// Any other mouse button: safely clear hover state (optional)
	UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
}

void UInv_InventoryGrid::CreateItemPopUp(const int32 GridIndex)
{
	if (!GridSlots.IsValidIndex(GridIndex) || !IsValid(GridSlots[GridIndex])) return;

	if (!ItemPopUpClass) return;

	UInv_InventoryItem* RightClickedItem = GridSlots[GridIndex]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;

	// Prevent duplicates for that slot
	if (IsValid(GridSlots[GridIndex]->GetItemPopUp())) return;

	APlayerController* PC = GetOwningPlayer();
	if (!IsValid(PC) || !ItemPopUpClass) return;

	ItemPopUp = CreateWidget<UInv_ItemPopUp>(PC, ItemPopUpClass);
	if (!IsValid(ItemPopUp)) return;

	GridSlots[GridIndex]->SetItemPopUp(ItemPopUp);

	// Put popup on screen (no Canvas dependency)
	ItemPopUp->AddToPlayerScreen(/*ZOrder*/ 500);
	ItemPopUp->SetVisibility(ESlateVisibility::Visible); // or SelfHitTestInvisible if only container should ignore hits
	ItemPopUp->SetIsEnabled(true);
	// Ensure DesiredSize is valid (important right after CreateWidget)
	ItemPopUp->ForceLayoutPrepass();

	// Position near mouse, but clamp to viewport
	FVector2D MousePos;
	if (!PC->GetMousePosition(MousePos.X, MousePos.Y))
	{
		int32 VX = 0, VY = 0;
		PC->GetViewportSize(VX, VY);
		MousePos = FVector2D(VX * 0.5f, VY * 0.5f);
	}

	const FVector2D PopupSize = ItemPopUp->GetDesiredSize();
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);

	// You previously used MousePosition - ItemPopUpOffset
	FVector2D Desired = MousePos - ItemPopUpOffset;

	// Clamp so it stays fully visible
	Desired.X = FMath::Clamp(Desired.X, 0.f, FMath::Max(0.f, ViewportSize.X - PopupSize.X));
	Desired.Y = FMath::Clamp(Desired.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - PopupSize.Y));

	// Top-left alignment is easier for clamp math
	ItemPopUp->SetAlignmentInViewport(FVector2D(0.f, 0.f));
	ItemPopUp->SetPositionInViewport(Desired, /*bRemoveDPIScale*/ true);

	// ---- existing logic unchanged ----
	const int32 SliderMax = GridSlots[GridIndex]->GetStackCount() - 1;
	if (RightClickedItem->IsStackable() && SliderMax > 0)
	{
		ItemPopUp->OnSplit.BindDynamic(this, &ThisClass::OnPopUpMenuSplit);
		ItemPopUp->SetSliderParams(SliderMax, FMath::Max(1, GridSlots[GridIndex]->GetStackCount() / 2));
	}
	else
	{
		ItemPopUp->CollapseSplitButton();
	}

	ItemPopUp->OnDrop.BindDynamic(this, &ThisClass::OnPopUpMenuDrop);

	if (RightClickedItem->IsConsumable())
	{
		ItemPopUp->OnConsume.BindDynamic(this, &ThisClass::OnPopUpMenuConsume);
	}
	else
	{
		ItemPopUp->CollapseConsumeButton();
	}
}

void UInv_InventoryGrid::PutHoverItemBack()
{
	if (!IsValid(HoverItem)) return;

	FInv_SlotAvailabilityResult Result = HasRoomForItem(HoverItem->GetInventoryItem(), HoverItem->GetStackCount());
	Result.Item = HoverItem->GetInventoryItem();

	AddStacks(Result);
	ClearHoverItem();
}

void UInv_InventoryGrid::DropItem()
{
	if (!IsValid(HoverItem)) return;
	if (!IsValid(HoverItem->GetInventoryItem())) return;

	InventoryComponent->Server_DropItem(HoverItem->GetInventoryItem(), HoverItem->GetStackCount());

	ClearHoverItem();
	ShowCursor();
}

bool UInv_InventoryGrid::HasHoverItem() const
{
	return IsValid(HoverItem);
}

UInv_HoverItem* UInv_InventoryGrid::GetHoverItem() const
{
	return HoverItem;
}

void UInv_InventoryGrid::AddItem(UInv_InventoryItem* Item)
{
	if (!MatchesCategory(Item)) return;

	FInv_SlotAvailabilityResult Result = HasRoomForItem(Item);
	AddItemToIndices(Result, Item);
}

void UInv_InventoryGrid::AddItemToIndices(const FInv_SlotAvailabilityResult& Result, UInv_InventoryItem* NewItem)
{
	for (const auto& Availability : Result.SlotAvailabilities)
	{
		AddItemAtIndex(NewItem, Availability.Index, Result.bStackable, Availability.AmountToFill);
		UpdateGridSlots(NewItem, Availability.Index, Result.bStackable, Availability.AmountToFill);
	}
}

void UInv_InventoryGrid::AddItemAtIndex(UInv_InventoryItem* Item, const int32 Index, const bool bStackable,
                                        const int32 StackAmount)
{
	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(Item, FragmentTags::GridFragment);
	const FInv_ImageFragment* ImageFragment = GetFragment<FInv_ImageFragment>(Item, FragmentTags::IconFragment);
	if (!GridFragment || !ImageFragment) return;

	UInv_SlottedItem* SlottedItem =
		CreateSlottedItem(Item, bStackable, StackAmount, GridFragment, ImageFragment, Index);
	AddSlottedItemToCanvas(Index, GridFragment, SlottedItem);

	SlottedItems.Add(Index, SlottedItem);
}

UInv_SlottedItem* UInv_InventoryGrid::CreateSlottedItem(UInv_InventoryItem* Item, const bool bStackable,
                                                        const int32 StackAmount, const FInv_GridFragment* GridFragment,
                                                        const FInv_ImageFragment* ImageFragment, const int32 Index)
{
	UInv_SlottedItem* SlottedItem = CreateWidget<UInv_SlottedItem>(GetOwningPlayer(), SlottedItemClass);
	SlottedItem->SetInventoryItem(Item);
	SetSlottedItemImage(SlottedItem, GridFragment, ImageFragment);
	SlottedItem->SetGridIndex(Index);
	SlottedItem->SetIsStackable(bStackable);
	const int32 StackUpdateAmount = bStackable ? StackAmount : 0;
	SlottedItem->UpdateStackCount(StackUpdateAmount);
	SlottedItem->OnSlottedItemClicked.AddDynamic(this, &ThisClass::OnSlottedItemClicked);

	return SlottedItem;
}

void UInv_InventoryGrid::AddSlottedItemToCanvas(const int32 Index, const FInv_GridFragment* GridFragment,
                                                UInv_SlottedItem* SlottedItem) const
{
	CanvasPanel->AddChild(SlottedItem);
	UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(SlottedItem);
	CanvasSlot->SetSize(GetDrawSize(GridFragment));
	const FVector2D DrawPos = UInv_WidgetUtils::GetPositionFromIndex(Index, Columns) * TileSize;
	const FVector2D DrawPosWithPadding = DrawPos + FVector2D(GridFragment->GetGridPadding());
	CanvasSlot->SetPosition(DrawPosWithPadding);
}

void UInv_InventoryGrid::UpdateGridSlots(UInv_InventoryItem* NewItem, const int32 Index, bool bStackableItem,
                                         const int32 StackAmount)
{
	check(GridSlots.IsValidIndex(Index));

	if (bStackableItem)
	{
		GridSlots[Index]->SetStackCount(StackAmount);
	}

	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(NewItem, FragmentTags::GridFragment);
	const FIntPoint Dimensions = GridFragment ? GridFragment->GetGridSize() : FIntPoint(1, 1);

	UInv_InventoryStatics::ForEach2D(GridSlots, Index, Dimensions, Columns, [&](UInv_GridSlot* GridSlot)
	{
		GridSlot->SetInventoryItem(NewItem);
		GridSlot->SetUpperLeftIndex(Index);
		GridSlot->SetOccupiedTexture();
		GridSlot->SetAvailable(false);
	});
}

bool UInv_InventoryGrid::IsIndexClaimed(const TSet<int32>& CheckedIndices, const int32 Index) const
{
	return CheckedIndices.Contains(Index);
}

FVector2D UInv_InventoryGrid::GetDrawSize(const FInv_GridFragment* GridFragment) const
{
	const float IconTileWidth = TileSize - GridFragment->GetGridPadding() * 2;
	return GridFragment->GetGridSize() * IconTileWidth;
}

void UInv_InventoryGrid::SetSlottedItemImage(const UInv_SlottedItem* SlottedItem, const FInv_GridFragment* GridFragment,
                                             const FInv_ImageFragment* ImageFragment) const
{
	FSlateBrush Brush;
	Brush.SetResourceObject(ImageFragment->GetIcon());
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = GetDrawSize(GridFragment);
	SlottedItem->SetImageBrush(Brush);
}

void UInv_InventoryGrid::ConstructGrid()
{
	GridSlots.Reserve(Rows * Columns);

	for (int32 j = 0; j < Rows; ++j)
	{
		for (int32 i = 0; i < Columns; ++i)
		{
			UInv_GridSlot* GridSlot = CreateWidget<UInv_GridSlot>(this, GridSlotClass);
			CanvasPanel->AddChild(GridSlot);

			const FIntPoint TilePosition(i, j);
			GridSlot->SetTileIndex(UInv_WidgetUtils::GetIndexFromPosition(TilePosition, Columns));

			UCanvasPanelSlot* GridCPS = UWidgetLayoutLibrary::SlotAsCanvasSlot(GridSlot);
			GridCPS->SetSize(FVector2D(TileSize));
			GridCPS->SetPosition(TilePosition * TileSize);

			GridSlots.Add(GridSlot);
			GridSlot->GridSlotClicked.AddDynamic(this, &ThisClass::OnGridSlotClicked);
			GridSlot->GridSlotHovered.AddDynamic(this, &ThisClass::OnGridSlotHovered);
			GridSlot->GridSlotUnhovered.AddDynamic(this, &ThisClass::OnGridSlotUnhovered);
		}
	}
}

void UInv_InventoryGrid::OnGridSlotClicked(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (UIMode == EInv_GridUIMode::ExternalInventory)
	{
		// external is not a place you can drop hover items into
		return;
	}

	if (!bPlayerOwnedInventory)
	{
		return;
	}

	if (!IsValid(HoverItem)) return;
	if (!GridSlots.IsValidIndex(ItemDropIndex)) return;

	if (CurrentQueryResult.ValidItem.IsValid() && GridSlots.IsValidIndex(CurrentQueryResult.UpperLeftIndex))
	{
		OnSlottedItemClicked(CurrentQueryResult.UpperLeftIndex, MouseEvent);
		return;
	}

	if (!IsInGridBounds(ItemDropIndex, HoverItem->GetGridDimensions())) return;
	auto GridSlot = GridSlots[ItemDropIndex];
	if (!GridSlot->GetInventoryItem().IsValid())
	{
		PutDownOnIndex(ItemDropIndex);
	}
}

void UInv_InventoryGrid::PutDownOnIndex(const int32 Index)
{
	AddItemAtIndex(HoverItem->GetInventoryItem(), Index, HoverItem->IsStackable(), HoverItem->GetStackCount());
	UpdateGridSlots(HoverItem->GetInventoryItem(), Index, HoverItem->IsStackable(), HoverItem->GetStackCount());
	ClearHoverItem();
}

void UInv_InventoryGrid::ClearHoverItem()
{
	if (!IsValid(HoverItem)) return;

	HoverItem->SetInventoryItem(nullptr);
	HoverItem->SetIsStackable(false);
	HoverItem->SetPreviousGridIndex(INDEX_NONE);
	HoverItem->UpdateStackCount(0);
	HoverItem->SetImageBrush(FSlateNoResource());

	HoverItem->RemoveFromParent();
	HoverItem = nullptr;

	ShowCursor();
}

UUserWidget* UInv_InventoryGrid::GetVisibleCursorWidget()
{
	if (!IsValid(GetOwningPlayer())) return nullptr;
	if (!IsValid(VisibleCursorWidget))
	{
		VisibleCursorWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), VisibleCursorWidgetClass);
	}
	return VisibleCursorWidget;
}

UUserWidget* UInv_InventoryGrid::GetHiddenCursorWidget()
{
	if (!IsValid(GetOwningPlayer())) return nullptr;
	if (!IsValid(HiddenCursorWidget))
	{
		HiddenCursorWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), HiddenCursorWidgetClass);
	}
	return HiddenCursorWidget;
}

bool UInv_InventoryGrid::IsSameStackable(const UInv_InventoryItem* ClickedInventoryItem) const
{
	if (!IsValid(ClickedInventoryItem) || !IsValid(HoverItem))
	{
		return false;
	}

	UInv_InventoryItem* HoverInvItem = HoverItem->GetInventoryItem();
	if (!IsValid(HoverInvItem))
	{
		return false;
	}

	if (!ClickedInventoryItem->IsStackable() || !HoverInvItem->IsStackable())
	{
		return false;
	}

	const FName ClickedID = ClickedInventoryItem->GetItemID();
	const FName HoverID = HoverInvItem->GetItemID();

	return !ClickedID.IsNone() && ClickedID == HoverID;
}

void UInv_InventoryGrid::SwapWithHoverItem(UInv_InventoryItem* ClickedInventoryItem, const int32 GridIndex)
{
	if (!IsValid(HoverItem)) return;

	UInv_InventoryItem* TempInventoryItem = HoverItem->GetInventoryItem();
	const int32 TempStackCount = HoverItem->GetStackCount();
	const bool bTempIsStackable = HoverItem->IsStackable();

	// Keep the same previous grid index
	AssignHoverItem(ClickedInventoryItem, GridIndex, HoverItem->GetPreviousGridIndex());
	RemoveItemFromGrid(ClickedInventoryItem, GridIndex);
	AddItemAtIndex(TempInventoryItem, ItemDropIndex, bTempIsStackable, TempStackCount);
	UpdateGridSlots(TempInventoryItem, ItemDropIndex, bTempIsStackable, TempStackCount);
}

bool UInv_InventoryGrid::ShouldSwapStackCounts(const int32 RoomInClickedSlot, const int32 HoveredStackCount,
                                               const int32 MaxStackSize) const
{
	return RoomInClickedSlot == 0 && HoveredStackCount < MaxStackSize;
}

void UInv_InventoryGrid::SwapStackCounts(const int32 ClickedStackCount, const int32 HoveredStackCount,
                                         const int32 Index)
{
	UInv_GridSlot* GridSlot = GridSlots[Index];
	GridSlot->SetStackCount(HoveredStackCount);

	UInv_SlottedItem* ClickedSlottedItem = SlottedItems.FindChecked(Index);
	ClickedSlottedItem->UpdateStackCount(HoveredStackCount);

	HoverItem->UpdateStackCount(ClickedStackCount);
}

bool UInv_InventoryGrid::ShouldConsumeHoverItemStacks(const int32 HoveredStackCount,
                                                      const int32 RoomInClickedSlot) const
{
	return RoomInClickedSlot >= HoveredStackCount;
}

void UInv_InventoryGrid::ConsumeHoverItemStacks(const int32 ClickedStackCount, const int32 HoveredStackCount,
                                                const int32 Index)
{
	const int32 AmountToTransfer = HoveredStackCount;
	const int32 NewClickedStackCount = ClickedStackCount + AmountToTransfer;

	GridSlots[Index]->SetStackCount(NewClickedStackCount);
	SlottedItems.FindChecked(Index)->UpdateStackCount(NewClickedStackCount);
	ClearHoverItem();
	ShowCursor();

	const FInv_GridFragment* GridFragment = GridSlots[Index]->GetInventoryItem()->GetItemManifest().GetFragmentOfType<
		FInv_GridFragment>();
	const FIntPoint Dimensions = GridFragment ? GridFragment->GetGridSize() : FIntPoint(1, 1);
	HighlightSlots(Index, Dimensions);
}

bool UInv_InventoryGrid::ShouldFillInStack(const int32 RoomInClickedSlot, const int32 HoveredStackCount) const
{
	return RoomInClickedSlot < HoveredStackCount;
}

void UInv_InventoryGrid::FillInStack(const int32 FillAmount, const int32 Remainder, const int32 Index)
{
	UInv_GridSlot* GridSlot = GridSlots[Index];
	const int32 NewStackCount = GridSlot->GetStackCount() + FillAmount;

	GridSlot->SetStackCount(NewStackCount);

	UInv_SlottedItem* ClickedSlottedItem = SlottedItems.FindChecked(Index);
	ClickedSlottedItem->UpdateStackCount(NewStackCount);

	HoverItem->UpdateStackCount(Remainder);
}

void UInv_InventoryGrid::ShowCursor()
{
	if (!IsValid(GetOwningPlayer())) return;

	// If we're dragging, keep the drag cursor
	if (IsValid(HoverItem))
	{
		GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, GetVisibleCursorWidget());
		return;
	}

	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, GetVisibleCursorWidget());
}

void UInv_InventoryGrid::HideCursor()
{
	if (!IsValid(GetOwningPlayer())) return;
	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, GetHiddenCursorWidget());
}

void UInv_InventoryGrid::SetOwningCanvas(UCanvasPanel* OwningCanvas)
{
	OwningCanvasPanel = OwningCanvas;
}

void UInv_InventoryGrid::OnGridSlotHovered(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (IsValid(HoverItem)) return;

	UInv_GridSlot* GridSlot = GridSlots[GridIndex];
	if (GridSlot->IsAvailable())
	{
		GridSlot->SetOccupiedTexture();
	}
}

void UInv_InventoryGrid::OnGridSlotUnhovered(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (IsValid(HoverItem)) return;

	UInv_GridSlot* GridSlot = GridSlots[GridIndex];
	if (GridSlot->IsAvailable())
	{
		GridSlot->SetUnoccupiedTexture();
	}
}

void UInv_InventoryGrid::OnPopUpMenuSplit(int32 SplitAmount, int32 Index)
{
	UInv_InventoryItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;
	if (!RightClickedItem->IsStackable()) return;

	const int32 UpperLeftIndex = GridSlots[Index]->GetUpperLeftIndex();
	UInv_GridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
	const int32 StackCount = UpperLeftGridSlot->GetStackCount();
	const int32 NewStackCount = StackCount - SplitAmount;

	UpperLeftGridSlot->SetStackCount(NewStackCount);
	SlottedItems.FindChecked(UpperLeftIndex)->UpdateStackCount(NewStackCount);

	AssignHoverItem(RightClickedItem, UpperLeftIndex, UpperLeftIndex);
	HoverItem->UpdateStackCount(SplitAmount);
}

void UInv_InventoryGrid::OnPopUpMenuDrop(int32 Index)
{
	UInv_InventoryItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;

	PickUp(RightClickedItem, Index);
	DropItem();
}

void UInv_InventoryGrid::OnPopUpMenuConsume(int32 Index)
{
	UInv_InventoryItem* RightClickedItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(RightClickedItem)) return;

	const int32 UpperLeftIndex = GridSlots[Index]->GetUpperLeftIndex();
	UInv_GridSlot* UpperLeftGridSlot = GridSlots[UpperLeftIndex];
	const int32 NewStackCount = UpperLeftGridSlot->GetStackCount() - 1;

	UpperLeftGridSlot->SetStackCount(NewStackCount);
	SlottedItems.FindChecked(UpperLeftIndex)->UpdateStackCount(NewStackCount);

	InventoryComponent->Server_ConsumeItem(RightClickedItem);

	if (NewStackCount <= 0)
	{
		RemoveItemFromGrid(RightClickedItem, Index);
	}
}

void UInv_InventoryGrid::OnInventoryMenuToggled(bool bOpen)
{
	if (!bOpen)
	{
		PutHoverItemBack();

		// Close any popups that might remain
		for (UInv_GridSlot* GridSlot : GridSlots)
		{
			if (!IsValid(GridSlot)) continue;

			if (UInv_ItemPopUp* Pop = GridSlot->GetItemPopUp())
			{
				Pop->RemoveFromParent();
				GridSlot->SetItemPopUp(nullptr);
			}
		}
	}
}

bool UInv_InventoryGrid::MatchesCategory(const UInv_InventoryItem* Item) const
{
	return Item->GetItemManifest().GetItemCategory() == ItemCategory;
}
