#include "Quest/Integration/QuestIntegrationComponent.h"

#include "Game/ProdigyGameState.h"
#include "GameFramework/Actor.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Net/UnrealNetwork.h"
#include "ProdigyInventory/InventoryComponent.h"

class AProdigyGameState;

void UQuestIntegrationComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	InventoryComp = Owner->FindComponentByClass<UInventoryComponent>();

	// fallback once (your code is fine)
	if (!IsValid(InventoryComp))
	{
		if (APlayerController* PC = Cast<APlayerController>(Owner))
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				InventoryComp = Pawn->FindComponentByClass<UInventoryComponent>();
			}
		}
		else if (APawn* Pawn = Cast<APawn>(Owner))
		{
			if (APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
			{
				InventoryComp = Controller->FindComponentByClass<UInventoryComponent>();
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[QuestIntegration] BeginPlay Owner=%s InventoryComp=%s"),
		*GetNameSafe(Owner),
		*GetNameSafe(InventoryComp));

	if (IsValid(InventoryComp))
	{
		InventoryComp->OnItemIDChanged.AddDynamic(this, &UQuestIntegrationComponent::HandleItemChanged);
	}
}

UQuestIntegrationComponent::UQuestIntegrationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false); // Adapter only; QuestLog replicates.
}

void UQuestIntegrationComponent::HandleItemChanged(FName ItemID)
{
	if (!ItemID.IsNone())
	{
		OnQuestItemChanged.Broadcast(ItemID);
	}
}

int32 UQuestIntegrationComponent::GetTotalQuantityByItemID_Implementation(FName ItemID) const
{
	if (ItemID.IsNone() || !IsValid(InventoryComp)) return 0;
	return InventoryComp->GetTotalQuantityByItemID(ItemID);
}

bool UQuestIntegrationComponent::AddItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context)
{
	if (ItemID.IsNone() || Quantity <= 0) return false;
	if (!IsValid(InventoryComp)) return false;

	// Full-only reward policy
	if (!InventoryComp->CanFullyAdd(ItemID, Quantity))
	{
		return false;
	}

	int32 Remainder = 0;
	TArray<int32> Changed;
	const bool bAddedAny = InventoryComp->AddItem(ItemID, Quantity, Remainder, Changed);

	return bAddedAny && (Remainder == 0);
}

void UQuestIntegrationComponent::RemoveItemByID_Implementation(FName ItemID, int32 Quantity, UObject* Context)
{
	if (ItemID.IsNone() || Quantity <= 0) return;
	if (!IsValid(InventoryComp)) return;

	// All-or-nothing turn-in policy
	if (!InventoryComp->CanRemoveByItemID(ItemID, Quantity))
	{
		return;
	}

	int32 Removed = 0;
	TArray<int32> Changed;
	InventoryComp->RemoveByItemID(ItemID, Quantity, Removed, Changed);
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
