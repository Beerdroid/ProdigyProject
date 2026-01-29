
#include "Items/Manifest/Inv_ItemManifest.h"

#include "Items/Inv_InventoryItem.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Widgets/Composite/Inv_CompositeBase.h"

UInv_InventoryItem* FInv_ItemManifest::Manifest(UObject* NewOuter)
{
	UInv_InventoryItem* Item = NewObject<UInv_InventoryItem>(NewOuter, UInv_InventoryItem::StaticClass());
	Item->SetItemManifest(*this);
	for (auto& Fragment : Item->GetItemManifestMutable().GetFragmentsMutable())
	{
		if (!Fragment.IsValid()) continue;
		Fragment.GetMutable().Manifest();
	}
	ClearFragments();
	
	return Item;
}

UInv_InventoryItem* FInv_ItemManifest::ManifestCopy(UObject* NewOuter) const
{
	UInv_InventoryItem* Item = NewObject<UInv_InventoryItem>(NewOuter, UInv_InventoryItem::StaticClass());
	Item->SetItemManifest(*this);

	// Run "Manifest()" on the item's own copy
	for (TInstancedStruct<FInv_ItemFragment>& Fragment : Item->GetItemManifestMutable().GetFragmentsMutable())
	{
		if (!Fragment.IsValid()) continue;
		Fragment.GetMutable().Manifest();
	}

	return Item;
}

void FInv_ItemManifest::AssimilateInventoryFragments(UInv_CompositeBase* Composite) const
{
	const auto& InventoryItemFragments = GetAllFragmentsOfType<FInv_InventoryItemFragment>();
	for (const auto* Fragment : InventoryItemFragments)
	{
		Composite->ApplyFunction([Fragment](UInv_CompositeBase* Widget)
		{
			Fragment->Assimilate(Widget);
		});
	}
}

void FInv_ItemManifest::SpawnPickupActor(const UObject* WorldContextObject, FName ItemID, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	if (!IsValid(PickupActorClass) || !IsValid(WorldContextObject)) return;
	if (ItemID.IsNone()) return;

	UWorld* World = WorldContextObject->GetWorld();
	if (!IsValid(World)) return;

	AActor* SpawnedActor = World->SpawnActor<AActor>(PickupActorClass, SpawnLocation, SpawnRotation);
	if (!IsValid(SpawnedActor)) return;

	UInv_ItemComponent* ItemComp = SpawnedActor->FindComponentByClass<UInv_ItemComponent>();
	if (!IsValid(ItemComp))
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnPickupActor: SpawnedActor %s has no UInv_ItemComponent"), *GetNameSafe(SpawnedActor));
		SpawnedActor->Destroy();
		return;
	}

	// Identity first
	ItemComp->SetItemID(ItemID);

	// Then instance data
	ItemComp->InitItemManifest(*this);
}


void FInv_ItemManifest::ClearFragments()
{
	for (auto& Fragment : Fragments)
	{ 
		Fragment.Reset();
	}
	Fragments.Empty();
}

