#pragma once

#include "CombatSubsystem.generated.h"

UCLASS()
class PRODIGYPROJECT_API UCombatSubsystem : public UWorldSubsystem
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

private:
	bool bInCombat = false;
	int32 TurnIndex = 0;
	TArray<TWeakObjectPtr<AActor>> Participants;
};

