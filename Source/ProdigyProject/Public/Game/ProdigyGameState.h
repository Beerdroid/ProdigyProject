#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ProdigyInventory/InventoryItemDBProvider.h"
#include "ProdigyGameState.generated.h"

class UQuestDatabase;
class UProdigyItemDatabase;

UCLASS()
class PRODIGYPROJECT_API AProdigyGameState : public AGameStateBase, public IInventoryItemDBProvider
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category="Quests")
	UQuestDatabase* GetQuestDatabase() const { return QuestDatabase; }

	virtual bool GetItemRowByID_Implementation(
		FName ItemID,
		FItemRow& OutRow
	) const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Items")
	TObjectPtr<UQuestDatabase> QuestDatabase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Items")
	TObjectPtr<UDataTable> ItemDataTable;

};
