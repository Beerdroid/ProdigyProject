#pragma once

#include "CombatSubsystem.generated.h"

struct FActionContext;

UCLASS()
class PRODIGYPROJECT_API UCombatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void EnterCombat(const TArray<AActor*>& InParticipants, AActor* FirstToAct);

	bool IsValidCombatant(AActor* A) const;

	UFUNCTION(BlueprintCallable)
	void ExitCombat();

	void BeginTurnForIndex(int32 Index);

	UFUNCTION(BlueprintCallable)
	bool IsInCombat() const { return bInCombat; }

	AActor* GetCurrentTurnActor() const;

	UFUNCTION(BlueprintCallable)
	void AdvanceTurn(); // call after an action resolves

	UFUNCTION()
	void HandleActionExecuted(FGameplayTag ActionTag, const FActionContext& Context);

	UFUNCTION(BlueprintCallable, Category="Combat")
	bool EndCurrentTurn(AActor* Instigator);

	void PruneParticipants();

	TArray<TWeakObjectPtr<AActor>> GetParticipants(){return Participants;}
	
private:
	bool bInCombat = false;
	int32 TurnIndex = 0;
	TArray<TWeakObjectPtr<AActor>> Participants;

	UPROPERTY()
	bool bAdvancingTurn = false;
};

