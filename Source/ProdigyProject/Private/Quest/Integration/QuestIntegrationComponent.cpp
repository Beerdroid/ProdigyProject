#include "Quest/Integration/QuestIntegrationComponent.h"

#include "GameFramework/Actor.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Net/UnrealNetwork.h"

void UQuestIntegrationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		if (UInv_InventoryComponent* Inv = Owner->FindComponentByClass<UInv_InventoryComponent>())
		{
			Inv->OnInvDelta.AddUObject(this, &UQuestIntegrationComponent::HandleInvDelta);
		}
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
	// Server-authoritative semantics: quest progress recompute should read from server inventory.
	// For standalone/authority this is fine; on clients this may return 0 unless you mirror inventory.
	// QuestLog should only recompute on server anyway.
	return BP_GetTotalQuantityByItemID(ItemID);
}

bool UQuestIntegrationComponent::AddItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context)
{
	if (ItemID.IsNone() || Quantity <= 0)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	const bool bSuccess = BP_AddItemByID(ItemID, Quantity, Context);
	if (bSuccess)
	{
		BroadcastInventoryDelta(ItemID, +Quantity, Context);
	}

	return bSuccess;
}

int32 UQuestIntegrationComponent::RemoveItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context)
{
	if (ItemID.IsNone() || Quantity <= 0)
	{
		return 0;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return 0;
	}

	const int32 Removed = BP_RemoveItemByID(ItemID, Quantity, Context);
	if (Removed > 0)
	{
		BroadcastInventoryDelta(ItemID, -Removed, Context);
	}

	return Removed;
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

	BP_AddCurrency(Amount);
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

	BP_AddXP(Amount);
}
