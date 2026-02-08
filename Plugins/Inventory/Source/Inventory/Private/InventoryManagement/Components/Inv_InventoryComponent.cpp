// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryManagement/Components/Inv_InventoryComponent.h"

#include "GameFramework/GameStateBase.h"
#include "Interfaces/Inv_ItemManifestProvider.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Widgets/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Net/UnrealNetwork.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Fragments/Inv_FragmentTags.h"
#include "Items/Fragments/Inv_ItemFragment.h"


static void CenterWidgetInViewport(
	UUserWidget* Widget,
	APlayerController* PC,
	FVector2D CenterOffsetPx = FVector2D::ZeroVector,
	UUserWidget* RelativeTo = nullptr,
	float GapPx = 0.f)
{
	if (!IsValid(Widget) || !IsValid(PC)) return;

	int32 VX = 0, VY = 0;
	PC->GetViewportSize(VX, VY);

	const FVector2D ViewportCenter(VX * 0.5f, VY * 0.5f);

	// Make sure layout info exists
	Widget->ForceLayoutPrepass();
	if (RelativeTo)
	{
		RelativeTo->ForceLayoutPrepass();
	}

	const FVector2D ThisSize = Widget->GetDesiredSize();
	const FVector2D OtherSize = RelativeTo ? RelativeTo->GetDesiredSize() : FVector2D::ZeroVector;

	// Center pivot
	Widget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));

	FVector2D FinalPos = ViewportCenter + CenterOffsetPx;

	// If we want to place relative to another widget (left/right)
	if (RelativeTo && GapPx != 0.f)
	{
		const float Dir = FMath::Sign(GapPx); // negative = left, positive = right
		const float AbsGap = FMath::Abs(GapPx);

		FinalPos.X += Dir * ((OtherSize.X * 0.5f) + (ThisSize.X * 0.5f) + AbsGap);
	}

	Widget->SetPositionInViewport(FinalPos, true);
}

UInv_InventoryComponent::UInv_InventoryComponent() : InventoryList(this)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
	bInventoryMenuOpen = false;
}

void UInv_InventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, InventoryList);
	DOREPLIFETIME(ThisClass, bInitialized);
	DOREPLIFETIME(ThisClass, PredefinedItems);
}

FName UInv_InventoryComponent::ResolveItemIDFromInventoryItem(const UInv_InventoryItem* Item) const
{
	return Item ? Item->GetItemID() : NAME_None;
}

int32 UInv_InventoryComponent::GetTotalQuantityByItemID(FName ItemID) const
{
	return InventoryList.GetTotalQuantityByItemID(ItemID);
}

FInv_ItemView UInv_InventoryComponent::BuildItemViewFromManifest(const FInv_ItemManifest& Manifest) const
{
	FInv_ItemView View;
	View.Category = Manifest.GetItemCategory();
	View.ItemType  = Manifest.GetItemType();

	// DisplayName
	if (const FInv_TextFragment* NameFrag =
		Manifest.GetFragmentOfTypeWithTag<FInv_TextFragment>(FragmentTags::ItemNameFragment))
	{
		View.DisplayName = NameFrag->GetText();
	}

	// Description (use FlavorTextFragment, NOT FragmentTags.UI.Description)
	if (const FInv_TextFragment* DescFrag =
		Manifest.GetFragmentOfTypeWithTag<FInv_TextFragment>(FragmentTags::FlavorTextFragment))
	{
		View.Description = DescFrag->GetText();
	}

	// Icon
	if (const FInv_ImageFragment* IconFrag =
		Manifest.GetFragmentOfTypeWithTag<FInv_ImageFragment>(FragmentTags::IconFragment))
	{
		View.Icon = IconFrag->GetIcon();
	}

	// Stackable
	if (const FInv_StackableFragment* StackFrag =
		Manifest.GetFragmentOfTypeWithTag<FInv_StackableFragment>(FragmentTags::StackableFragment))
	{
		View.bStackable = true;
		View.MaxStack   = StackFrag->GetMaxStackSize();
	}
	else
	{
		View.bStackable = false;
		View.MaxStack   = 1;
	}

	return View;
}

bool UInv_InventoryComponent::TryAddItemByManifest(FName ItemID, const FInv_ItemManifest& Manifest, int32 Quantity, int32& OutRemainder)
{
	UE_LOG(LogTemp, Warning, TEXT("TryAddItemByManifest: Owner=%s Auth=%d MenuValid=%d Item=%s Qty=%d"),
	*GetNameSafe(GetOwner()),
	GetOwner() ? (int32)GetOwner()->HasAuthority() : -1,
	(int32)IsValid(InventoryMenu),
	*ItemID.ToString(),
	Quantity);

		
	OutRemainder = Quantity;
	if (ItemID.IsNone() || Quantity <= 0) return false;

	// If we have UI, use your existing capacity logic
	if (IsValid(InventoryMenu))
	{
		FInv_SlotAvailabilityResult Result = InventoryMenu->HasRoomForItem(Manifest, Quantity);
		UInv_InventoryItem* FoundItem = InventoryList.FindFirstItemByID(ItemID);
		Result.Item = FoundItem;

		if (Result.TotalRoomToFill == 0)
		{
		UE_LOG(LogTemp, Warning, TEXT("TryAddItemByManifest: NO ROOM (TotalRoomToFill=0) for %s"), *ItemID.ToString());
			
			NoRoomInInventory.Broadcast();
			return false;
		}


		OutRemainder = Result.Remainder;

		if (Result.Item.IsValid() && Result.bStackable)
		{
			OnStackChange.Broadcast(Result);
			Server_AddStacksToItemFromManifest(ItemID, Manifest.GetItemType(), Result.TotalRoomToFill);
		}
		else
		{
			const int32 CountToCreate = Result.bStackable ? Result.TotalRoomToFill : 1;
			Server_AddNewItemFromManifest(ItemID, Manifest, CountToCreate);
		}

		UE_LOG(LogTemp, Warning, TEXT("TryAddItemByManifest UIResult: TotalRoomToFill=%d Remainder=%d bStackable=%d FoundItem=%s"),
	Result.TotalRoomToFill,
	Result.Remainder,
	(int32)Result.bStackable,
	*GetNameSafe(Result.Item.Get()));

		return (Result.TotalRoomToFill > 0);
	}

	// ---- Server-safe fallback (no UI) ----
	// For quests/rewards: just add all. (Capacity rules must be moved out of UI later if you need them.)
	const bool bStackable =
		Manifest.GetFragmentOfTypeWithTag<FInv_StackableFragment>(FragmentTags::StackableFragment) != nullptr;

	OutRemainder = 0;

	if (bStackable)
	{
		// Put whole quantity into your single “total stack count” model
		if (UInv_InventoryItem* Existing = InventoryList.FindFirstItemByID(ItemID))
		{
			Server_AddStacksToItemFromManifest(ItemID, Manifest.GetItemType(), Quantity);
		}
		else
		{
			Server_AddNewItemFromManifest(ItemID, Manifest, Quantity);
		}
	}
	else
	{
		// Non-stackable: add Quantity separate entries
		for (int32 i = 0; i < Quantity; ++i)
		{
			Server_AddNewItemFromManifest(ItemID, Manifest, 1);
		}
	}

	return true;
}

void UInv_InventoryComponent::Client_EmitInvDelta_Implementation(FName ItemID, int32 DeltaQty, EInv_MoveReason Reason)
{
	OnInvDelta.Broadcast(ItemID, DeltaQty, /*Context*/ nullptr);
}

void UInv_InventoryComponent::Server_AddNewItemFromManifest_Implementation(FName ItemID, FInv_ItemManifest Manifest, int32 StackCount)
{
	if (ItemID.IsNone() || StackCount <= 0) return;

	UInv_InventoryItem* NewItem = InventoryList.AddEntryFromManifest(Manifest);
	if (!IsValid(NewItem)) return;

	NewItem->SetItemID(ItemID);
	NewItem->SetTotalStackCount(StackCount);

	// 🔥 Sync manifest fragment stack so UI that reads fragment shows correct value
	if (FInv_StackableFragment* StackFrag =
		NewItem->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackFrag->SetStackCount(StackCount);
	}

	EmitInvDeltaByItemID(ItemID, +StackCount, this);

	if (GetOwner()->GetNetMode() == NM_ListenServer || GetOwner()->GetNetMode() == NM_Standalone)
	{
		OnItemAdded.Broadcast(NewItem);
	}
}

void UInv_InventoryComponent::Server_AddStacksToItemFromManifest_Implementation(FName ItemID, FGameplayTag ItemType, int32 StackCount)
{
	if (ItemID.IsNone() || StackCount <= 0) return;

	UInv_InventoryItem* Item = InventoryList.FindFirstItemByID(ItemID);
	if (!IsValid(Item)) return;

	if (Item->GetItemID().IsNone())
	{
		Item->SetItemID(ItemID);
	}

	const int32 NewTotal = Item->GetTotalStackCount() + StackCount;
	Item->SetTotalStackCount(NewTotal);

	// 🔥 Sync fragment stack
	if (FInv_StackableFragment* StackFrag =
		Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackFrag->SetStackCount(NewTotal);
	}

	EmitInvDeltaByItemID(ItemID, +StackCount, this);
}

void UInv_InventoryComponent::TryAddItem(UInv_ItemComponent* ItemComponent)
{
	FInv_SlotAvailabilityResult Result = InventoryMenu->HasRoomForItem(ItemComponent);

	UInv_InventoryItem* FoundItem = InventoryList.FindFirstItemByID(ItemComponent->GetItemID());
	Result.Item = FoundItem;

	if (Result.TotalRoomToFill == 0)
	{
		NoRoomInInventory.Broadcast();
		return;
	}
	
	if (Result.Item.IsValid() && Result.bStackable)
	{
		// Add stacks to an item that already exists in the inventory. We only want to update the stack count,
		// not create a new item of this type.
		OnStackChange.Broadcast(Result);
		Server_AddStacksToItem(ItemComponent, Result.TotalRoomToFill, Result.Remainder);
	}
	else if (Result.TotalRoomToFill > 0)
	{
		// For non-stackables, each pickup actor represents exactly one item.
		const int32 CountToCreate = Result.bStackable ? Result.TotalRoomToFill : 1;
		Server_AddNewItem(ItemComponent, CountToCreate, Result.Remainder);
	}

	UE_LOG(LogTemp, Warning, TEXT("INV AFTER PICKUP: Entries=%d"), InventoryList.GetAllItems().Num());
	for (const auto& E : InventoryList.GetAllItems())
	{
		UE_LOG(LogTemp, Warning, TEXT("  Entry: ItemID=%s Qty=%d"),
			*E->GetItemID().ToString(),
			E->GetTotalStackCount());
	}
}

bool UInv_InventoryComponent::AddItemByID_ServerAuth(FName ItemID, int32 Quantity, UObject* Context,
	int32& OutRemainder)
{
	OutRemainder = Quantity;

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AddByID] FAIL: not authority Owner=%s"), *GetNameSafe(Owner));
		return false;
	}

	if (ItemID.IsNone() || Quantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AddByID] FAIL: invalid args ItemID=%s Qty=%d"), *ItemID.ToString(), Quantity);
		return false;
	}

	// 1) Resolve manifest via interface provider (GameState implements it)
	FInv_ItemManifest Manifest;
	if (!ResolveManifestByItemID(ItemID, Manifest))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AddByID] FAIL: manifest not found ItemID=%s"), *ItemID.ToString());
		return false;
	}

	// 2) Add using the proven NoUI logic (server-safe, deterministic)
	const bool bOk = TryAddItemByManifest_NoUI(ItemID, Manifest, Quantity, OutRemainder);

	UE_LOG(LogTemp, Warning,
		TEXT("[AddByID] ItemID=%s Qty=%d -> Ok=%d Remainder=%d Context=%s"),
		*ItemID.ToString(),
		Quantity,
		(int32)bOk,
		OutRemainder,
		*GetNameSafe(Context)
	);

	// If you want deltas/quest updates, you already EmitInvDelta in the server add functions.
	// Nothing else needed here.

	return bOk;
}

void UInv_InventoryComponent::Server_RemoveItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context)
{
	if (ItemID.IsNone() || Quantity <= 0) return;
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	const bool bLocalServerUI =
		(GetOwner()->GetNetMode() == NM_ListenServer || GetOwner()->GetNetMode() == NM_Standalone);

	int32 Remaining = Quantity;
	int32 RemovedTotal = 0;

	while (Remaining > 0)
	{
		UInv_InventoryItem* Item = InventoryList.FindFirstItemByID(ItemID);
		if (!IsValid(Item))
		{
			break;
		}

		const int32 CurrentStack = Item->GetTotalStackCount();
		if (CurrentStack <= 0)
		{
			// Broken entry, remove it.
			if (bLocalServerUI)
			{
				OnItemRemoved.Broadcast(Item);
			}

			InventoryList.RemoveEntry(Item);
			continue;
		}

		const int32 Take = FMath::Min(CurrentStack, Remaining);
		const int32 NewStack = CurrentStack - Take;

		if (NewStack <= 0)
		{
			// IMPORTANT: notify local UI before removing (listen/standalone won't get PreReplicatedRemove)
			if (bLocalServerUI)
			{
				OnItemRemoved.Broadcast(Item);
			}

			InventoryList.RemoveEntry(Item);
		}
		else
		{
			Item->SetTotalStackCount(NewStack);

			// If your stack count is NOT replicated via the item subobject, you may also need a “stack changed”
			// notification here. But often UI reads directly from the item and updates automatically on listen server.
		}

		Remaining -= Take;
		RemovedTotal += Take;
	}

	if (RemovedTotal > 0)
	{
		EmitInvDeltaByItemID(ItemID, -RemovedTotal, Context);
	}
}

void UInv_InventoryComponent::Server_AddNewItem_Implementation(UInv_ItemComponent* ItemComponent, int32 StackCount, int32 Remainder)
{
	if (!IsValid(ItemComponent) || StackCount <= 0) return;

	UInv_InventoryItem* NewItem = InventoryList.AddEntry(ItemComponent);
	if (!IsValid(NewItem)) return;

	const FName ItemID = ItemComponent->GetItemID();
	NewItem->SetItemID(ItemID);
	NewItem->SetTotalStackCount(StackCount);

	if (StackCount > 0)
	{
		EmitInvDeltaByItemID(ItemID, +StackCount, this);
	}

	if (GetOwner()->GetNetMode() == NM_ListenServer || GetOwner()->GetNetMode() == NM_Standalone)
	{
		OnItemAdded.Broadcast(NewItem);
	}

	if (Remainder == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FInv_StackableFragment* StackableFragment =
		ItemComponent->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackableFragment->SetStackCount(Remainder);
	}
}

void UInv_InventoryComponent::Server_AddStacksToItem_Implementation(UInv_ItemComponent* ItemComponent, int32 StackCount, int32 Remainder)
{
	if (!IsValid(ItemComponent) || StackCount <= 0) return;

	const FGameplayTag ItemType = ItemComponent->GetItemManifest().GetItemType();
	UInv_InventoryItem* Item = InventoryList.FindFirstItemByID(ItemComponent->GetItemID());
	if (!IsValid(Item)) return;

	const FName ItemID = ItemComponent->GetItemID();
	if (Item->GetItemID().IsNone())
	{
		Item->SetItemID(ItemID);
	}

	Item->SetTotalStackCount(Item->GetTotalStackCount() + StackCount);

	EmitInvDeltaByItemID(ItemID, +StackCount, this);

	if (Remainder == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FInv_StackableFragment* StackableFragment = ItemComponent->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackableFragment->SetStackCount(Remainder);
	}
}

void UInv_InventoryComponent::Server_DropItem_Implementation(UInv_InventoryItem* Item, int32 StackCount)
{
	const int32 NewStackCount = Item->GetTotalStackCount() - StackCount;
	if (NewStackCount <= 0)
	{
		InventoryList.RemoveEntry(Item);
	}
	else
	{
		Item->SetTotalStackCount(NewStackCount);
	}

	const FName ItemID = ResolveItemIDFromInventoryItem(Item);
	EmitInvDeltaByItemID(ItemID, -StackCount, this);

	SpawnDroppedItem(Item, StackCount);
}

void UInv_InventoryComponent::SpawnDroppedItem(UInv_InventoryItem* Item, int32 StackCount)
{
	const APawn* OwningPawn = OwningController->GetPawn();
	FVector RotatedForward = OwningPawn->GetActorForwardVector();
	RotatedForward = RotatedForward.RotateAngleAxis(FMath::FRandRange(DropSpawnAngleMin, DropSpawnAngleMax), FVector::UpVector);
	FVector SpawnLocation = OwningPawn->GetActorLocation() + RotatedForward * FMath::FRandRange(DropSpawnDistanceMin, DropSpawnDistanceMax);
	SpawnLocation.Z -= RelativeSpawnElevation;
	const FRotator SpawnRotation = FRotator::ZeroRotator;

	FInv_ItemManifest& ItemManifest = Item->GetItemManifestMutable();
	if (FInv_StackableFragment* StackableFragment = ItemManifest.GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackableFragment->SetStackCount(StackCount);
	}
	const FName ItemID = Item->GetItemID();
	ItemManifest.SpawnPickupActor(this, ItemID, SpawnLocation, SpawnRotation);
}


void UInv_InventoryComponent::SyncExternalInventoryToUI()
{
	if (!IsValid(ExternalInventoryMenu) || !IsValid(ExternalInventoryComp)) return;

	ExternalInventoryMenu->SetSourceInventory(ExternalInventoryComp.Get());
}

void UInv_InventoryComponent::EmitInvDeltaByItemID(FName ItemID, int32 DeltaQty, UObject* Context)
{
	if (ItemID.IsNone() || DeltaQty == 0) return;

	// Always broadcast on whichever machine is executing this call
	OnInvDelta.Broadcast(ItemID, DeltaQty, Context);

	// If we are authoritative, also forward to owning client so UI/quest can update there
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	// Determine reason safely (you can map from Context if you want; simplest is pass nullptr and choose reason at call sites)
	const EInv_MoveReason Reason = EInv_MoveReason::Move;

	// Only the owning client needs this
	Client_EmitInvDelta(ItemID, DeltaQty, Reason);
}

void UInv_InventoryComponent::Server_ConsumeItem_Implementation(UInv_InventoryItem* Item)
{
	const int32 NewStackCount = Item->GetTotalStackCount() - 1;
	if (NewStackCount <= 0)
	{
		InventoryList.RemoveEntry(Item);
	}
	else
	{
		Item->SetTotalStackCount(NewStackCount);
	}

	const FName ItemID = ResolveItemIDFromInventoryItem(Item);
	EmitInvDeltaByItemID(ItemID, -1, this);

	if (FInv_ConsumableFragment* ConsumableFragment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_ConsumableFragment>())
	{
		ConsumableFragment->OnConsume(OwningController.Get());
	}
}

void UInv_InventoryComponent::Server_EquipSlotClicked_Implementation(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip)
{
	Multicast_EquipSlotClicked(ItemToEquip, ItemToUnequip);
}

void UInv_InventoryComponent::Multicast_EquipSlotClicked_Implementation(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip)
{
	// Equipment Component will listen to these delegates
	OnItemEquipped.Broadcast(ItemToEquip);
	OnItemUnequipped.Broadcast(ItemToUnequip);
}

void UInv_InventoryComponent::ToggleInventoryMenu()
{
	if (bInventoryMenuOpen)
	{
		CloseInventoryMenu();
	}
	else
	{
		OpenInventoryMenu();
	}
	OnInventoryMenuToggled.Broadcast(bInventoryMenuOpen);
}

void UInv_InventoryComponent::AddRepSubObj(UObject* SubObj)
{
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && IsValid(SubObj))
	{
		AddReplicatedSubObject(SubObj);
	}
}

void UInv_InventoryComponent::HandleExternalInvDelta(FName ItemID, int32 DeltaQty, UObject* Context)
{
	if (!bExternalMenuOpen) return;
	if (!IsValid(ExternalInventoryMenu)) return;

	// Optional: only refresh if the delta came from the currently opened external comp
	ExternalInventoryComp->ReplayInventoryToUI();
}

TArray<UInv_InventoryItem*> UInv_InventoryComponent::GetAllItems()
{
	return InventoryList.GetAllItems();
}

void UInv_InventoryComponent::EnsureExternalInventoryMenuCreated()
{
	if (!OwningController.IsValid()) return;

	if (IsValid(ExternalInventoryMenu))
	{
		return;
	}

	TSubclassOf<UInv_InventoryBase> WidgetClass =
		ExternalInventoryMenuClass ? ExternalInventoryMenuClass : InventoryMenuClass;

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InvUI] ExternalInventoryMenuClass and InventoryMenuClass are null"));
		return;
	}

	ExternalInventoryMenu = CreateWidget<UInv_InventoryBase>(OwningController.Get(), WidgetClass);
	if (!IsValid(ExternalInventoryMenu)) return;

	ExternalInventoryMenu->AddToPlayerScreen(20);
	ExternalInventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
	ExternalInventoryMenu->SetIsEnabled(false);

	APlayerController* PC = OwningController.Get();
	PC->GetWorldTimerManager().SetTimerForNextTick([this, PC]()
	{
		if (IsValid(ExternalInventoryMenu))
		{
			CenterWidgetInViewport(ExternalInventoryMenu, PC, FVector2D(-400.f, 0.f));
		}
	});

	UE_LOG(LogTemp, Warning, TEXT("[InvUI] ExternalInventoryMenu created: %s"), *GetNameSafe(ExternalInventoryMenu));
}

void UInv_InventoryComponent::OpenExternalInventoryUI(UInv_InventoryComponent* InExternal)
{
	UE_LOG(LogTemp, Warning, TEXT("OpenExternalInventoryUI started"));

	if (!OwningController.IsValid()) return;
	if (!IsValid(InExternal)) return;

	// Unbind previous external
	if (IsValid(InExternal) && InExternal->ExternalInventoryComp != this)
	{
		InExternal->ExternalInventoryComp = this;
		InExternal->bExternalMenuOpen = true; // optional, but helps if you gate logic by this
	}


	ExternalInventoryComp = InExternal;
	bExternalMenuOpen = true;

	if (!IsValid(InventoryMenu))
	{
		ConstructInventory();
	}
	if (!IsValid(InventoryMenu)) return;

	EnsureExternalInventoryMenuCreated();
	if (!IsValid(ExternalInventoryMenu)) return;

	// Bind to external changes
	ExternalInventoryComp->OnInvDelta.AddUObject(this, &UInv_InventoryComponent::HandleExternalInvDelta);

	ExternalInventoryMenu->SetSourceInventory(ExternalInventoryComp);

	// IMPORTANT: refresh the visible widget, not the merchant component
	ExternalInventoryComp->ReplayInventoryToUI();

	InventoryMenu->SetIsEnabled(true);
	InventoryMenu->SetVisibility(ESlateVisibility::Visible);

	ExternalInventoryMenu->SetIsEnabled(true);
	ExternalInventoryMenu->SetVisibility(ESlateVisibility::Visible);

	bInventoryMenuOpen = true;

	ApplyGameAndUIInputMode();

	UE_LOG(LogTemp, Warning, TEXT("OpenExternalInventoryUI completed"));
}

void UInv_InventoryComponent::CloseExternalInventoryUI()
{
	if (!OwningController.IsValid()) return;

	// Hide external menu if exists
	if (IsValid(ExternalInventoryMenu))
	{
		ExternalInventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
		ExternalInventoryMenu->SetIsEnabled(false);

		// Optional: only if your widget actually expects to unbind/release references
		ExternalInventoryMenu->SetSourceInventory(nullptr);
	}

	ExternalInventoryComp = nullptr;
	bExternalMenuOpen = false; // <-- add this flag (recommended)

	// DO NOT touch InventoryMenu.
	// DO NOT touch bInventoryMenuOpen.

	// Only restore GameOnly if *no other UI* should keep GameAndUI mode.
	if (!bInventoryMenuOpen /* && !AnyOtherUiOpen() */)
	{
		ApplyGameOnlyInputMode();
	}
	// else: keep current input mode, mouse cursor, etc.
}

void UInv_InventoryComponent::ApplyGameOnlyInputMode()
{
	if (!OwningController.IsValid()) return;

	UE_LOG(LogTemp, Warning, TEXT("ApplyGameOnlyInputMode 1"));

	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);

	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(false);

	// These are optional depending on your click-to-move setup
	OwningController->bEnableClickEvents = true;
	OwningController->bEnableMouseOverEvents = true;

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
		GEngine->GameViewport->SetHideCursorDuringCapture(false);
	}
}

void UInv_InventoryComponent::EnsurePredefinedApplied()
{
	UE_LOG(LogTemp, Warning, TEXT("EnsurePredefinedApplied started"));

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority()) return;

	if (bPredefinedApplied) return;

	// Nothing to do, but we still want to stop retrying forever.
	if (PredefinedItems.Num() == 0)
	{
		bPredefinedApplied = true;
		Owner->FlushNetDormancy();
		Owner->ForceNetUpdate();
		return;
	}

	Server_InitializeFromPredefinedItems_Implementation();

	// Mark AFTER initialization.
	bPredefinedApplied = true;

	UE_LOG(LogTemp, Warning, TEXT("EnsurePredefinedApplied completed"));
}

void UInv_InventoryComponent::ReplayInventoryToUI()
{
	const TArray<UInv_InventoryItem*> Items = InventoryList.GetAllItems();

	UE_LOG(LogTemp, Warning, TEXT("[InvUI] ReplayInventoryToUI Owner=%s Items=%d"),
		*GetNameSafe(GetOwner()), Items.Num());

	for (UInv_InventoryItem* Item : Items)
	{
		if (!IsValid(Item)) continue;

		UE_LOG(LogTemp, Warning, TEXT("[InvUI]  Replay Item=%s ItemID=%s Stack=%d"),
			*GetNameSafe(Item),
			*Item->GetItemID().ToString(),
			Item->GetTotalStackCount());

		OnItemAdded.Broadcast(Item);
	}
}

bool UInv_InventoryComponent::BelongsTo(const APlayerController* PC) const
{
	if (!IsValid(PC)) return false;
	if (!OwningController.IsValid()) return false;

	return OwningController.Get() == PC;
}

bool UInv_InventoryComponent::BelongsToOwningController() const
{
	return OwningController.IsValid() && (OwningController.Get() == Cast<APlayerController>(OwningController.Get()));
	// (This is basically "is valid"; keep if you want semantic readability)
}

void UInv_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	ConstructInventory();              // player local UI only
	InventoryList.OwnerComponent = this;

}

void UInv_InventoryComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	InventoryList.OwnerComponent = this;

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	if (bInitialized) return;
	if (!IsReadyForReplication()) return;

	UE_LOG(LogTemp, Warning, TEXT("[InvInit] ReadyForReplication Seed Owner=%s"), *GetNameSafe(Owner));

	Server_InitializeFromPredefinedItems_Implementation();

	bInitialized = true;

	Owner->FlushNetDormancy();
	Owner->ForceNetUpdate();
}

void UInv_InventoryComponent::NotifyInventoryChanged()
{
	OnInventoryChanged.Broadcast();
}

void UInv_InventoryComponent::SyncInventoryToUI()
{
	if (!IsValid(InventoryMenu)) return;

	const TArray<UInv_InventoryItem*> Items = InventoryList.GetAllItems();

	UE_LOG(LogTemp, Warning, TEXT("[InvUI] SyncInventoryToUI Owner=%s Items=%d"),
		*GetNameSafe(GetOwner()), Items.Num());

	for (UInv_InventoryItem* Item : Items)
	{
		if (!IsValid(Item)) continue;

		UE_LOG(LogTemp, Warning, TEXT("[InvUI]  Replay Add Item=%s ItemID=%s Stack=%d"),
			*GetNameSafe(Item),
			*Item->GetItemID().ToString(),
			Item->GetTotalStackCount());

		OnItemAdded.Broadcast(Item);
	}
}

bool UInv_InventoryComponent::ResolveManifestByItemID(FName ItemID, FInv_ItemManifest& OutManifest) const
{
	if (ItemID.IsNone()) return false;
	if (!GetWorld()) return false;

	AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS) return false;

	if (!GS->GetClass()->ImplementsInterface(UInv_ItemManifestProvider::StaticClass()))
	{
		return false;
	}

	return IInv_ItemManifestProvider::Execute_FindItemManifest(GS, ItemID, OutManifest);
}

void UInv_InventoryComponent::Server_InitializeFromPredefinedItems_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("Server_InitializeFromPredefinedItems started"));
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	if (PredefinedItems.Num() == 0) return;

	UE_LOG(LogTemp, Warning,
	TEXT("[PredefInit] Owner=%s Auth=%d Items=%d"),
	*GetNameSafe(GetOwner()),
	GetOwner()->HasAuthority(),
	PredefinedItems.Num()
);

	for (const FInv_PredefinedItemEntry& Entry : PredefinedItems)
	{
		UE_LOG(LogTemp, Warning,
	TEXT("[PredefInit] Add ItemID=%s Qty=%d"),
	*Entry.ItemID.ToString(),
	Entry.Quantity
);
		if (Entry.ItemID.IsNone() || Entry.Quantity <= 0)
		{
			continue;
		}

		// Resolve manifest from ItemID (must use your existing item DB)
		FInv_ItemManifest Manifest;
		if (!ResolveManifestByItemID(Entry.ItemID, Manifest))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("InitializeFromPredefinedItems: Manifest not found for ItemID=%s Owner=%s"),
				*Entry.ItemID.ToString(),
				*GetNameSafe(Owner));
			continue;
		}

		int32 Remainder = 0;

		// Reuses ALL your existing logic:
		// - UI path if InventoryMenu exists (player inventory)
		// - Server-only fallback otherwise (NPC / chest / merchant)
		TryAddItemByManifest_NoUI(Entry.ItemID, Manifest, Entry.Quantity, Remainder);

		if (Remainder > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("InitializeFromPredefinedItems: Remainder=%d ItemID=%s Owner=%s"),
				Remainder,
				*Entry.ItemID.ToString(),
				*GetNameSafe(Owner));
		}
	}

}


void UInv_InventoryComponent::Server_InitializeMerchantStock_Implementation()
{
}

bool UInv_InventoryComponent::TryAddItemByManifest_NoUI(
	FName ItemID,
	const FInv_ItemManifest& Manifest,
	int32 Quantity,
	int32& OutRemainder)
{
	AActor* Owner = GetOwner();

	UE_LOG(LogTemp, Warning,
		TEXT("[AddNoUI] BEGIN Owner=%s Auth=%d ItemID=%s Qty=%d EntriesBefore=%d"),
		*GetNameSafe(Owner),
		Owner ? (int32)Owner->HasAuthority() : -1,
		*ItemID.ToString(),
		Quantity,
		InventoryList.GetAllItems().Num()
	);

	OutRemainder = Quantity;

	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AddNoUI] ABORT: Owner=null"));
		return false;
	}

	if (ItemID.IsNone() || Quantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AddNoUI] ABORT: Invalid ItemID or Qty (ItemID=%s Qty=%d)"),
			*ItemID.ToString(), Quantity);
		return false;
	}

	if (!Owner->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AddNoUI] ABORT: Not authority (Owner=%s)"), *GetNameSafe(Owner));
		return false;
	}

	const FInv_StackableFragment* StackFrag =
		Manifest.GetFragmentOfTypeWithTag<FInv_StackableFragment>(FragmentTags::StackableFragment);

	const bool bStackable = (StackFrag != nullptr);

	// Pre-state for this item
	UInv_InventoryItem* Existing0 = InventoryList.FindFirstItemByID(ItemID);

	UE_LOG(LogTemp, Warning,
		TEXT("[AddNoUI] PreState ItemID=%s bStackable=%d Existing=%s ExistingTotalStack=%d TotalQtyByID=%d"),
		*ItemID.ToString(),
		(int32)bStackable,
		*GetNameSafe(Existing0),
		Existing0 ? Existing0->GetTotalStackCount() : -1,
		GetTotalQuantityByItemID(ItemID)
	);

	OutRemainder = 0;

	if (bStackable)
	{
		if (UInv_InventoryItem* Existing = InventoryList.FindFirstItemByID(ItemID))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[AddNoUI] STACKABLE -> AddStacks ItemID=%s Add=%d ItemType=%s BeforeStack=%d"),
				*ItemID.ToString(),
				Quantity,
				*Manifest.GetItemType().ToString(),
				Existing->GetTotalStackCount()
			);

			Server_AddStacksToItemFromManifest(ItemID, Manifest.GetItemType(), Quantity);

			UInv_InventoryItem* After = InventoryList.FindFirstItemByID(ItemID);
			UE_LOG(LogTemp, Warning,
				TEXT("[AddNoUI] STACKABLE After AddStacks Item=%s AfterStack=%d TotalQtyByID=%d"),
				*GetNameSafe(After),
				After ? After->GetTotalStackCount() : -1,
				GetTotalQuantityByItemID(ItemID)
			);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[AddNoUI] STACKABLE -> AddNew ItemID=%s StackCount=%d ItemType=%s"),
				*ItemID.ToString(),
				Quantity,
				*Manifest.GetItemType().ToString()
			);

			Server_AddNewItemFromManifest(ItemID, Manifest, Quantity);

			UInv_InventoryItem* After = InventoryList.FindFirstItemByID(ItemID);
			UE_LOG(LogTemp, Warning,
				TEXT("[AddNoUI] STACKABLE After AddNew Item=%s ItemID=%s AfterStack=%d TotalQtyByID=%d"),
				*GetNameSafe(After),
				After ? *After->GetItemID().ToString() : TEXT("<null>"),
				After ? After->GetTotalStackCount() : -1,
				GetTotalQuantityByItemID(ItemID)
			);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AddNoUI] NON-STACKABLE -> loop create Qty=%d ItemID=%s"),
			Quantity,
			*ItemID.ToString()
		);

		for (int32 i = 0; i < Quantity; ++i)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[AddNoUI] NON-STACKABLE AddNew i=%d ItemID=%s StackCount=1"),
				i,
				*ItemID.ToString()
			);

			Server_AddNewItemFromManifest(ItemID, Manifest, 1);
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[AddNoUI] NON-STACKABLE After loop TotalQtyByID=%d"),
			GetTotalQuantityByItemID(ItemID)
		);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[AddNoUI] END ItemID=%s EntriesAfter=%d TotalQtyByID=%d"),
		*ItemID.ToString(),
		InventoryList.GetAllItems().Num(),
		GetTotalQuantityByItemID(ItemID)
	);

	// Dump current inventory snapshot (helps catch "multiple entries, each 1")
	for (UInv_InventoryItem* It : InventoryList.GetAllItems())
	{
		if (!IsValid(It)) continue;

		UE_LOG(LogTemp, Warning,
			TEXT("[AddNoUI]   Entry Item=%s ItemID=%s TotalStack=%d"),
			*GetNameSafe(It),
			*It->GetItemID().ToString(),
			It->GetTotalStackCount()
		);
	}

	NotifyInventoryChanged();

	return true;
}

void UInv_InventoryComponent::ApplyGameAndUIInputMode()
{
	if (!OwningController.IsValid()) return;

	UE_LOG(LogTemp, Warning, TEXT("ApplyGameOnlyInputMode 2"));

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(true);

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
		GEngine->GameViewport->SetHideCursorDuringCapture(false);
	}
}

void UInv_InventoryComponent::ConstructInventory()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Unified player path: owner can be PlayerController OR Pawn.
	APlayerController* PC = Cast<APlayerController>(Owner);
	if (!PC)
	{
		if (APawn* PawnOwner = Cast<APawn>(Owner))
		{
			PC = Cast<APlayerController>(PawnOwner->GetController());
		}
	}

	// If still null => NPC pawn, server-owned actor, merchant actor, etc. No local UI here.
	if (!IsValid(PC) || !PC->IsLocalController())
	{
		return;
	}

	OwningController = PC;

	InventoryMenu = CreateWidget<UInv_InventoryBase>(PC, InventoryMenuClass);
	if (!IsValid(InventoryMenu)) return;

	InventoryMenu->AddToPlayerScreen(10);
	InventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
	InventoryMenu->SetIsEnabled(false);

	PC->GetWorldTimerManager().SetTimerForNextTick([this, PC]()
	{
		if (IsValid(InventoryMenu))
		{
			CenterWidgetInViewport(InventoryMenu, PC, FVector2D(400.f, 0.f));
		}
	});
	bInventoryMenuOpen = false;

	SyncInventoryToUI();
}


void UInv_InventoryComponent::OpenInventoryMenu()
{
	if (!IsValid(InventoryMenu) || !OwningController.IsValid()) return;

	UE_LOG(LogTemp, Warning, TEXT("ApplyGameOnlyInputMode 3"));

	InventoryMenu->SetIsEnabled(true);
	InventoryMenu->SetVisibility(ESlateVisibility::Visible);
	bInventoryMenuOpen = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(true);

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
		GEngine->GameViewport->SetHideCursorDuringCapture(false);
	}
}

void UInv_InventoryComponent::CloseInventoryMenu()
{
	if (!IsValid(InventoryMenu) || !OwningController.IsValid()) return;

	InventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
	InventoryMenu->SetIsEnabled(false);
	bInventoryMenuOpen = false;

	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	OwningController->SetInputMode(InputMode);
	UE_LOG(LogTemp, Warning, TEXT("ApplyGameOnlyInputMode 4"));

	OwningController->SetShowMouseCursor(true);
	OwningController->bEnableClickEvents = true;
	OwningController->bEnableMouseOverEvents = true;

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
		GEngine->GameViewport->SetHideCursorDuringCapture(false);
	}
}
