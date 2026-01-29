#include "Data/DataBase/ProdigyItemDatabase.h"

void UProdigyItemDatabase::BuildCacheIfNeeded() const
{
	if (bCacheBuilt) return;

	IndexByID.Reset();
	IndexByID.Reserve(Items.Num());

	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FName ID = Items[i].ItemID;
		if (ID.IsNone()) continue;

		// If duplicates exist, first wins. You can swap behavior if preferred.
		if (!IndexByID.Contains(ID))
		{
			IndexByID.Add(ID, i);
		}
	}

	bCacheBuilt = true;
}

bool UProdigyItemDatabase::FindManifest(FName ItemID, FInv_ItemManifest& OutManifest) const
{
	if (ItemID.IsNone())
	{
		return false;
	}

	BuildCacheIfNeeded();

	if (const int32* FoundIndex = IndexByID.Find(ItemID))
	{
		if (Items.IsValidIndex(*FoundIndex))
		{
			OutManifest = Items[*FoundIndex].Manifest; // copy out
			return true;
		}
	}

	// Fallback (should rarely run, but safe if cache got stale in editor)
	for (const FProdigyItemRecord& R : Items)
	{
		if (R.ItemID == ItemID)
		{
			OutManifest = R.Manifest;
			return true;
		}
	}

	return false;
}
