#include "Quest/Integration/QuestIntegrationComponent.h"

#include "Game/ProdigyGameState.h"
#include "GameFramework/Actor.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Net/UnrealNetwork.h"

class AProdigyGameState;

void UQuestIntegrationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		if (UInv_InventoryComponent* Inv = Owner->FindComponentByClass<UInv_InventoryComponent>())
		{
			InventoryComp = Inv;
			InventoryComp->OnInvDelta.AddUObject(this, &UQuestIntegrationComponent::HandleInvDelta);
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
	OutManifest = FInv_ItemManifest();
	if (ItemID.IsNone()) return false;

	const UWorld* World = GetWorld();
	if (!World) return false;

	const AProdigyGameState* GS = World->GetGameState<AProdigyGameState>();
	if (!GS) return false;

	return GS->FindItemManifest(ItemID, OutManifest);
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
	if (ItemID.IsNone()) return 0;

	return InventoryComp->GetTotalQuantityByItemID(ItemID);
}

bool UQuestIntegrationComponent::AddItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context)
{
	UE_LOG(LogTemp, Warning, TEXT("QuestIntegration AddItemByID Item=%s Qty=%d Auth=%d"),
		*ItemID.ToString(), Quantity, GetOwner() ? (int32)GetOwner()->HasAuthority() : -1);

	if (ItemID.IsNone() || Quantity <= 0)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	if (!IsValid(InventoryComp))
	{
		UE_LOG(LogTemp, Error, TEXT("QuestIntegration AddItemByID: InventoryComp missing (Owner=%s)"),
			*GetNameSafe(OwnerActor));
		return false;
	}

	// Manifest lookup (GS / DT)
	FInv_ItemManifest Manifest;
	if (!TryGetItemManifest(ItemID, Manifest))
	{
		UE_LOG(LogTemp, Error, TEXT("QuestIntegration AddItemByID: Manifest not found for %s"), *ItemID.ToString());
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Spawn a transient actor to host the item component (so PickedUp() destroys THIS actor, not PC)
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = OwnerActor; // optional

	AActor* TempPickupActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!IsValid(TempPickupActor))
	{
		UE_LOG(LogTemp, Error, TEXT("QuestIntegration AddItemByID: Failed to spawn temp pickup actor"));
		return false;
	}

	TempPickupActor->SetActorEnableCollision(false);
	TempPickupActor->SetReplicates(false);

	// Create the item component owned by the temp actor
	UInv_ItemComponent* TempItemComp = NewObject<UInv_ItemComponent>(TempPickupActor, TEXT("TempQuestRewardItemComp"));
	if (!IsValid(TempItemComp))
	{
		TempPickupActor->Destroy();
		return false;
	}

	// Make ownership/lifecycle explicit (optional but recommended)
	TempPickupActor->AddInstanceComponent(TempItemComp);

	// Register => GetOwner() becomes TempPickupActor and PickedUp()->Destroy() is safe
	TempItemComp->RegisterComponent();

	// Initialize like a real pickup
	TempItemComp->InitItemManifest(Manifest);
	TempItemComp->SetItemID(ItemID);

	// Put reward quantity into the stack fragment so pickup logic sees correct stack/remainder
	if (FInv_StackableFragment* StackFrag =
		TempItemComp->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackFrag->SetStackCount(Quantity);
	}

	UE_LOG(LogTemp, Warning, TEXT("QuestIntegration AddItemByID: TempActor=%s CompOwner=%s Registered=%d"),
		*GetNameSafe(TempPickupActor),
		*GetNameSafe(TempItemComp->GetOwner()),
		(int32)TempItemComp->IsRegistered());

	// TryAddItem is void -> verify via totals
	const int32 Before = InventoryComp->GetTotalQuantityByItemID(ItemID);

	InventoryComp->TryAddItem(TempItemComp);

	const int32 After = InventoryComp->GetTotalQuantityByItemID(ItemID);
	const int32 Added = After - Before;

	const bool bOk = (Added == Quantity);

	UE_LOG(LogTemp, Warning, TEXT("QuestIntegration AddItemByID: Before=%d After=%d Added=%d Ok=%d"),
		Before, After, Added, (int32)bOk);

	// Policy for quest rewards:
	// If not fully added, destroy temp actor so you don't leave invisible pickups around.
	// (If you want "drop remainder", DON'T destroy here; instead keep it and position it.)
	if (!bOk && IsValid(TempPickupActor))
	{
		TempPickupActor->Destroy();
	}

	if (bOk)
	{
		BroadcastInventoryDelta(ItemID, +Quantity, Context);
	}

	return bOk;
}

void UQuestIntegrationComponent::RemoveItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context)
{
	if (ItemID.IsNone() || Quantity <= 0) return;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()) return;

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
