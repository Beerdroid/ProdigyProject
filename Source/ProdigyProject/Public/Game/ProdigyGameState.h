#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Interfaces/Inv_ItemManifestProvider.h"
#include "Items/Manifest/Inv_ItemManifest.h"   
#include "ProdigyGameState.generated.h"

class UQuestDatabase;
class UProdigyItemDatabase;

UCLASS()
class PRODIGYPROJECT_API AProdigyGameState : public AGameStateBase, public IInv_ItemManifestProvider
{
	GENERATED_BODY()

public:
	// Assign in BP_GameState_Prodigy (details panel)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Items")
	TObjectPtr<UProdigyItemDatabase> ItemDatabase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Items")
	TObjectPtr<UQuestDatabase> QuestDatabase;


	virtual bool FindItemManifest_Implementation(
	FName ItemID,
	FInv_ItemManifest& OutManifest
) const override;

	UFUNCTION(BlueprintPure, Category="Quests")
	UQuestDatabase* GetQuestDatabase() const { return QuestDatabase; }

protected:
	virtual void BeginPlay() override;
};
