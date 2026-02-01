#include "Game/ProdigyGameState.h"

#include "Data/DataBase/ProdigyItemDatabase.h"

bool AProdigyGameState::FindItemManifest_Implementation(FName ItemID, FInv_ItemManifest& OutManifest) const
{
	if (!ItemDatabase)
	{
		return false;
	}
	return ItemDatabase->FindManifest(ItemID, OutManifest);
}

void AProdigyGameState::BeginPlay()
{
	Super::BeginPlay();

	// Optional: build cache early so the first BP call is cheap and predictable.
	if (ItemDatabase)
	{
		ItemDatabase->BuildCacheIfNeeded();
	}
}
