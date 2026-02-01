#include "Quest/Integration/QuestIntegrationComponent.h"

#include "Game/ProdigyGameState.h"
#include "GameFramework/Actor.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Net/UnrealNetwork.h"

class AProdigyGameState;

void UQuestIntegrationComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	InventoryComp = Owner->FindComponentByClass<UInv_InventoryComponent>();

	// If integration is on PC but inventory is on pawn (or vice versa), fallback once.
	if (!IsValid(InventoryComp))
	{
		if (APlayerController* PC = Cast<APlayerController>(Owner))
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				InventoryComp = Pawn->FindComponentByClass<UInv_InventoryComponent>();
			}
		}
		else if (APawn* Pawn = Cast<APawn>(Owner))
		{
			if (APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
			{
				InventoryComp = Controller->FindComponentByClass<UInv_InventoryComponent>();
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[QuestIntegration] BeginPlay Owner=%s InventoryComp=%s"),
		*GetNameSafe(Owner),
		*GetNameSafe(InventoryComp));

	if (IsValid(InventoryComp))
	{
		InventoryComp->OnInvDelta.AddUObject(this, &UQuestIntegrationComponent::HandleInvDelta);
	}
}

void UQuestIntegrationComponent::HandleInvDelta(FName ItemID, int32 DeltaQty, UObject* Context)
{
	BroadcastInventoryDelta(ItemID, DeltaQty, Context);
}

UQuestIntegrationComponent::UQuestIntegrationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false); // Adapter only; QuestLog replicates.
}

bool UQuestIntegrationComponent::GetItemViewByID_Implementation(FName ItemID, FInv_ItemView& OutView) const
{
	OutView = FInv_ItemView();

	if (ItemID.IsNone() || !IsValid(InventoryComp)) return false;

	// Prodigy-side manifest lookup
	FInv_ItemManifest Manifest;
	if (!TryGetItemManifest(ItemID, Manifest)) return false;   // your GS/data table lookup

	OutView = InventoryComp->BuildItemViewFromManifest(Manifest);
	return true;
}

bool UQuestIntegrationComponent::TryGetItemManifest(FName ItemID, FInv_ItemManifest& OutManifest) const
{
	if (ItemID.IsNone())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AGameStateBase* GS = World->GetGameState();
	if (!GS)
	{
		return false;
	}

	if (!GS->GetClass()->ImplementsInterface(UInv_ItemManifestProvider::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[QuestIntegration] GameState=%s does not implement Inv_ItemManifestProvider"),
			*GetNameSafe(GS));
		return false;
	}

	const bool bOk = IInv_ItemManifestProvider::Execute_FindItemManifest(GS, ItemID, OutManifest);

	UE_LOG(LogTemp, Warning,
		TEXT("[QuestIntegration] FindItemManifest ItemID=%s -> %d"),
		*ItemID.ToString(),
		(int32)bOk);

	return bOk;
}

void UQuestIntegrationComponent::BroadcastInventoryDelta(FName ItemID, int32 DeltaQty, UObject* Context)
{
	if (ItemID.IsNone() || DeltaQty == 0)
	{
		return;
	}

	FInventoryDelta D;
	D.ItemID = ItemID;
	D.DeltaQuantity = DeltaQty;
	D.Context = Context;

	OnQuestInventoryDelta.Broadcast(D);
}

int32 UQuestIntegrationComponent::GetTotalQuantityByItemID_Implementation(FName ItemID) const
{
	if (ItemID.IsNone() || !IsValid(InventoryComp)) return 0;
	return InventoryComp->GetTotalQuantityByItemID(ItemID);
}

bool UQuestIntegrationComponent::AddItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context)
{
	if (ItemID.IsNone() || Quantity <= 0) return false;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("[QuestIntegration] AddItemByID rejected (not authority) Owner=%s"),
			*GetNameSafe(OwnerActor));
		return false;
	}

	if (!IsValid(InventoryComp))
	{
		UE_LOG(LogTemp, Warning, TEXT("[QuestIntegration] AddItemByID failed: InventoryComp=None Owner=%s"),
			*GetNameSafe(OwnerActor));
		return false;
	}

	// If your AddItemByID_ServerAuth internally does manifest lookup, you can skip this.
	// If it needs manifest, keep lookup inside the inventory component. (Preferred.)
	int32 Remainder = 0;
	const bool bOk = InventoryComp->AddItemByID_ServerAuth(ItemID, Quantity, Context, Remainder);

	UE_LOG(LogTemp, Warning, TEXT("[QuestIntegration] AddItemByID ItemID=%s Qty=%d -> %d Rem=%d Inv=%s"),
		*ItemID.ToString(), Quantity, (int32)bOk, Remainder, *GetNameSafe(InventoryComp));

	return bOk;
}

void UQuestIntegrationComponent::RemoveItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context)
{
	if (ItemID.IsNone() || Quantity <= 0) return;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()) return;

	if (!IsValid(InventoryComp))
	{
		UE_LOG(LogTemp, Warning, TEXT("[QuestIntegration] RemoveItemByID failed: InventoryComp=None Owner=%s"),
			*GetNameSafe(OwnerActor));
		return;
	}

	InventoryComp->Server_RemoveItemByID(ItemID, Quantity, Context);
}

void UQuestIntegrationComponent::AddCurrency_Implementation(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

}

void UQuestIntegrationComponent::AddXP_Implementation(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

}
