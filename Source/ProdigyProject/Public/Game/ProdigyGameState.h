#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Items/Manifest/Inv_ItemManifest.h"   
#include "ProdigyGameState.generated.h"

class UQuestDatabase;
class UProdigyItemDatabase;

UCLASS()
class PRODIGYPROJECT_API AProdigyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	// Assign in BP_GameState_Prodigy (details panel)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Items")
	TObjectPtr<UProdigyItemDatabase> ItemDatabase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Items")
	TObjectPtr<UQuestDatabase> QuestDatabase;

	// BP can call this to get a manifest by ID.
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="Items")
	bool FindItemManifest(FName ItemID, FInv_ItemManifest& OutManifest) const;

	UFUNCTION(BlueprintPure, Category="Quests")
	UQuestDatabase* GetQuestDatabase() const { return QuestDatabase; }

protected:
	virtual void BeginPlay() override;
};
