#pragma once

#include "CombatSubsystem.generated.h"

struct FGameplayTag;
struct FActionContext;

UCLASS()
class PRODIGYPROJECT_API UCombatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	// Delay between end of one turn and begin of next turn (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Timing")
	float BetweenTurnsDelaySeconds = 1.00f;

	// Optional: delay before first BeginTurn after EnterCombat
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Timing")
	float EnterCombatDelaySeconds = 1.0f;

	// Optional: delay after ExitCombat request (mostly for debug)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Timing")
	float ExitCombatDelaySeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Timing")
	float AITakeActionDelaySeconds = 1.0f;

	// Call this instead of calling BeginTurn directly
	void ScheduleBeginTurnForIndex(int32 Index, float DelaySeconds);

	FTimerHandle TimerHandle_AITakeAction;
	
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

	FTimerHandle TimerHandle_BeginTurn;
	bool bTurnBeginScheduled = false;

	void CancelScheduledBeginTurn();
	void HandleScheduledBeginTurn();

	int32 PendingTurnIndex = INDEX_NONE;
	
	bool bInCombat = false;
	int32 TurnIndex = 0;
	TArray<TWeakObjectPtr<AActor>> Participants;

	UPROPERTY()
	bool bAdvancingTurn = false;
};

