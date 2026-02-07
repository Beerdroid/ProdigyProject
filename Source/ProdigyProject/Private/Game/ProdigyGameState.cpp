#include "Game/ProdigyGameState.h"

#include "ProdigyInventory/ItemTypes.h"

bool AProdigyGameState::GetItemRowByID_Implementation(FName ItemID, FItemRow& OutRow) const
{
	OutRow = FItemRow();

	if (ItemID.IsNone() || !IsValid(ItemDataTable)) return false;

	static const FString Context(TEXT("AProdigyGameState::GetItemRowByID"));

	// Recommended: RowName == ItemID (fast path)
	if (const FItemRow* Row = ItemDataTable->FindRow<FItemRow>(ItemID, Context, /*bWarn*/ false))
	{
		OutRow = *Row;
		return true;
	}

	// Optional fallback if your RowName != ItemID
	for (const auto& Pair : ItemDataTable->GetRowMap())
	{
		const FItemRow* R = reinterpret_cast<const FItemRow*>(Pair.Value);
		if (R && R->ItemID == ItemID)
		{
			OutRow = *R;
			return true;
		}
	}

	return false;
}
