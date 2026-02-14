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