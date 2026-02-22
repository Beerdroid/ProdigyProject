#pragma once

#include "CombatSubsystem.generated.h"

struct FGameplayTag;
struct FActionContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatTurnActorChanged, AActor*, CurrentTurnActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, bool, bNowInCombat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnParticipantsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCombatAITurnBegan, AActor*, AIActor, AActor*, TargetActor);

UCLASS()
class PRODIGYPROJECT_API UCombatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable, Category="Combat|Events")
	FOnCombatAITurnBegan OnAITurnBegan;

	// Call this from AI systems (BT/task/controller) to take over the AI turn.
	UFUNCTION(BlueprintCallable, Category="Combat|AI")
	void MarkAITurnHandled(AActor* AIActor);

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

	const TArray<TWeakObjectPtr<AActor>>& GetParticipants() const
	{
		return Participants;
	}

	UPROPERTY(BlueprintAssignable, Category="Combat|Events")
	FOnCombatTurnActorChanged OnTurnActorChanged;

	UPROPERTY(BlueprintAssignable, Category="Combat|Events")
	FOnCombatStateChanged OnCombatStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FOnParticipantsChanged OnParticipantsChanged;

	UFUNCTION(BlueprintCallable, Category="Combat|Query")
	AActor* GetCurrentTurnActor_BP() const { return GetCurrentTurnActor(); }

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void TryBindToWorldEvents(UWorld* World);
	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS);

	UFUNCTION()
	void HandleCombatStartRequested(AActor* AggroActor, AActor* TargetActor, AActor* FirstToAct);

	// NEW: damage fallback -> request combat
	UFUNCTION()
	void HandleWorldDamageEvent(AActor* TargetActor, AActor* InstigatorActor, float AppliedDamage, float OldHP, float NewHP);

	UFUNCTION()
	void HandleWorldEffectApplied(AActor* TargetActor, AActor* InstigatorActor, FGameplayTag ActionTag);

	UFUNCTION(BlueprintCallable, Category="Combat|Faction")
	static bool AreHostile(const AActor* A, const AActor* B);

	static FGameplayTag GetFactionTag(const AActor* Actor);

	bool IsPlayerDead() const;
	bool AreAllEnemiesDead() const;

	void TryEndCombat_Simple();

private:

	FTimerHandle TimerHandle_BeginTurn;
	bool bTurnBeginScheduled = false;

	void CancelScheduledBeginTurn();
	void HandleScheduledBeginTurn();

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentAITurnActor;

	UPROPERTY(Transient)
	bool bCurrentAITurnHandled = false;

	void BuildParticipantsForAggro(AActor* AggroActor, AActor* PlayerPawn, TArray<AActor*>& OutParticipants) const;

	// NEW: resolve single player pawn for “player first”
	AActor* ResolveSinglePlayerPawn() const;

	int32 PendingTurnIndex = INDEX_NONE;
	
	bool bInCombat = false;
	int32 TurnIndex = 0;
	TArray<TWeakObjectPtr<AActor>> Participants;

	UPROPERTY()
	bool bAdvancingTurn = false;

	bool bBoundToWorldEvents = false;

	TWeakObjectPtr<class UWorldCombatEvents> BoundWorldEvents;
};

