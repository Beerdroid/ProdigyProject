#include "AbilitySystem/CombatSubsystem.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "EngineUtils.h"
#include "AbilitySystem/ActionAgentInterface.h"
#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/ActionCueSubsystem.h"
#include "AbilitySystem/ProdigyAbilityUtils.h"
#include "AbilitySystem/ProdigyGameplayTags.h"
#include "AbilitySystem/WorldCombatEvents.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/EnemyCombatantCharacter.h"
#include "Character/AIController/EnemyAIController.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"

bool UCombatSubsystem::IsValidCombatant(AActor* A) const
{
	if (!IsValid(A)) return false;

	if (!A->FindComponentByClass<UActionComponent>())
	{
		return false;
	}

	if (ProdigyAbilityUtils::IsDeadByAttributes(A)) return false;

	if (!A->GetClass()->ImplementsInterface(UActionAgentInterface::StaticClass()))
	{
		UE_LOG(LogActionExec, Error, TEXT("[Combat] Invalid combatant %s: missing ActionAgentInterface"),
		       *GetNameSafe(A));
		return false;
	}

	const bool bHasHealth =
		IActionAgentInterface::Execute_HasAttribute(A, ProdigyTags::Attr::Health) &&
		IActionAgentInterface::Execute_HasAttribute(A, ProdigyTags::Attr::MaxHealth);

	const bool bHasAP =
		IActionAgentInterface::Execute_HasAttribute(A, ProdigyTags::Attr::AP) &&
		IActionAgentInterface::Execute_HasAttribute(A, ProdigyTags::Attr::MaxAP);

	if (!bHasHealth || !bHasAP)
	{
		UE_LOG(LogActionExec, Error,
		       TEXT(
			       "[Combat] Invalid combatant %s: missing required Attr tags (need Health/MaxHealth/AP/MaxAP in DefaultAttributes)"
		       ),
		       *GetNameSafe(A)
		);
		return false;
	}

	return true;
}


void UCombatSubsystem::ExitCombat()
{
	UE_LOG(LogActionExec, Warning, TEXT("[Combat] ExitCombat Started"));

	if (!bInCombat) return;

	// Stop any scheduled work first
	CancelScheduledBeginTurn();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_AITakeAction);
	}

	for (TWeakObjectPtr<AActor>& W : Participants)
	{
		APawn* P = Cast<APawn>(W.Get());
		if (!P) continue;

		if (AAIController* AIC = Cast<AAIController>(P->GetController()))
		{
			AIC->GetBrainComponent()->StopLogic(TEXT("Combat ended"));
		}
	}

	// Unbind + reset combat flags on current participants
	// (use the current array before we clear it)
	for (TWeakObjectPtr<AActor>& W : Participants)
	{
		AActor* A = W.Get();
		if (!IsValid(A)) continue;

		if (UActionComponent* AC = A->FindComponentByClass<UActionComponent>())
		{
			AC->SetInCombat(false);
			AC->OnActionExecuted.RemoveDynamic(this, &UCombatSubsystem::HandleActionExecuted);
		}
	}

	// ---- IMPORTANT: update state BEFORE broadcasting ----
	Participants.Reset();
	TurnIndex = 0;
	bAdvancingTurn = false;
	bInCombat = false;

	// Now notify listeners with correct, final state
	OnCombatStateChanged.Broadcast(false);
	OnTurnActorChanged.Broadcast(nullptr);
	OnParticipantsChanged.Broadcast();

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] ExitCombat Completed"));
}

void UCombatSubsystem::EnterCombat(const TArray<AActor*>& InParticipants, AActor* FirstToAct)
{
	// If we're already in combat, ignore
	if (bInCombat) return;

	// Build a filtered list first (do NOT flip bInCombat yet)
	TArray<TWeakObjectPtr<AActor>> NewParticipants;
	NewParticipants.Reserve(InParticipants.Num());

	for (AActor* A : InParticipants)
	{
		if (!IsValidCombatant(A)) continue;
		NewParticipants.Add(A);
	}

	// ✅ No combat if we don't have at least 2 combatants
	if (NewParticipants.Num() < 2)
	{
		UE_LOG(LogActionExec, Warning,
		       TEXT("[Combat] EnterCombat ABORTED: ValidParticipants=%d (need >= 2). FirstToAct=%s"),
		       NewParticipants.Num(), *GetNameSafe(FirstToAct));
		return;
	}


	// Now we can enter combat
	bInCombat = true;
	bAdvancingTurn = false;

	Participants = MoveTemp(NewParticipants);

	for (TWeakObjectPtr<AActor>& W : Participants)
	{
		AActor* A = W.Get();
		if (!IsValid(A)) continue;

		if (UActionComponent* AC = A->FindComponentByClass<UActionComponent>())
		{
			AC->SetInCombat(true);

			AC->OnActionExecuted.RemoveDynamic(this, &UCombatSubsystem::HandleActionExecuted);
			AC->OnActionExecuted.AddDynamic(this, &UCombatSubsystem::HandleActionExecuted);
		}
	}

	TurnIndex = Participants.IndexOfByKey(FirstToAct);
	if (TurnIndex == INDEX_NONE)
	{
		TurnIndex = 0;
	}

	OnCombatStateChanged.Broadcast(true);
	OnTurnActorChanged.Broadcast(GetCurrentTurnActor());

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] EnterCombat: Participants=%d FirstToAct=%s TurnIndex=%d"),
	       Participants.Num(), *GetNameSafe(FirstToAct), TurnIndex);

	for (int32 i = 0; i < Participants.Num(); ++i)
	{
		AActor* P = Participants[i].Get();
		UE_LOG(LogActionExec, Warning, TEXT("[Combat]  P[%d]=%s Valid=%d HasActionComp=%d"),
		       i,
		       *GetNameSafe(P),
		       IsValid(P),
		       IsValid(P) && P->FindComponentByClass<UActionComponent>() != nullptr);
	}


	ScheduleBeginTurnForIndex(TurnIndex, EnterCombatDelaySeconds);

	if (UActionCueSubsystem* Cues = GetWorld()->GetSubsystem<UActionCueSubsystem>())
	{
		FActionCueContext Ctx;
		Ctx.InstigatorActor = FirstToAct;
		Ctx.TargetActor = nullptr;
		Cues->PlayCue(ActionCueTags::Cue_Combat_Enter, Ctx);
	}
}

void UCombatSubsystem::BeginTurnForIndex(int32 Index)
{
	PruneParticipants();
	if (!bInCombat) return;

	// NEW: if last enemy died during delay, end now
	TryEndCombat_Simple();
	if (!bInCombat) return;

	const int32 Num = Participants.Num();
	if (Num <= 0) return;

	// ✅ Index can be invalid after pruning; normalize it
	Index = (Index % Num + Num) % Num;

	if (!Participants.IsValidIndex(Index))
	{
		UE_LOG(LogActionExec, Warning, TEXT("[Combat] BeginTurn: invalid index after normalize Index=%d Num=%d"), Index, Num);
		ExitCombat();
		return;
	}

	AActor* TurnActor = Participants[Index].Get();
	if (!IsValid(TurnActor))
	{
		UE_LOG(LogActionExec, Warning, TEXT("[Combat] BeginTurn: TurnActor invalid at Index=%d -> Prune+Advance"), Index);
		PruneParticipants();
		if (!bInCombat) return;
		AdvanceTurn();
		return;
	}

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] BeginTurn: Index=%d Actor=%s"),
		Index, *GetNameSafe(TurnActor));

	// Begin-turn work (AP refresh etc.)
	if (UActionComponent* TurnAC = TurnActor->FindComponentByClass<UActionComponent>())
	{
		TurnAC->OnTurnBegan();
	}

	OnTurnActorChanged.Broadcast(TurnActor);

	// Player waits for input
	APawn* PawnOwner = Cast<APawn>(TurnActor);
	const bool bIsPlayer = PawnOwner && PawnOwner->IsPlayerControlled();
	if (bIsPlayer)
	{
		UE_LOG(LogActionExec, Warning, TEXT("[Combat] Player turn: waiting for input"));
		return;
	}

	// ---- AI TURN ----

	// ✅ If this pawn is controlled by your EnemyAIController, run BT for the turn and exit.
	// (BT task should ExecuteAction and on failure call EndCurrentTurn(PawnOwner))
	if (PawnOwner)
	{
		if (AEnemyAIController* EnemyAIC = Cast<AEnemyAIController>(PawnOwner->GetController()))
		{
			// Pick a hostile target (faction-aware) from Participants
			AActor* Target = nullptr;
			for (const TWeakObjectPtr<AActor>& W : Participants)
			{
				AActor* A = W.Get();
				if (!IsValid(A) || A == TurnActor) continue;
				if (ProdigyAbilityUtils::IsDeadByAttributes(A)) continue;

				if (AreHostile(TurnActor, A))
				{
					Target = A;
					break;
				}
			}

			if (!IsValid(Target))
			{
				UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI turn: no hostile target -> passing"));
				AdvanceTurn();
				return;
			}

			// Write BB context for this turn
			if (UBlackboardComponent* BB = EnemyAIC->GetBlackboardComponent())
			{
				BB->SetValueAsObject(ProdigyBB::TargetActor, Target);
				BB->SetValueAsName(ProdigyBB::ActionTag, TEXT("Action.Attack.Basic")); // minimal default
			}
			else
			{
				UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI turn: no blackboard on %s -> fallback AI"),
					*GetNameSafe(EnemyAIC));
				// fall through to your fallback AI
			}

			UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI turn: starting BT Controller=%s Pawn=%s Target=%s"),
				*GetNameSafe(EnemyAIC), *GetNameSafe(PawnOwner), *GetNameSafe(Target));

			EnemyAIC->StartCombatTurn();
			return;
		}
	}

	// ---- Fallback: your current primitive AI (kept so nothing stalls if no BT/controller) ----

	// Pick a target among participants (first valid, non-self, non-dead)
	AActor* Target = nullptr;
	for (const TWeakObjectPtr<AActor>& W : Participants)
	{
		AActor* A = W.Get();
		if (!IsValid(A)) continue;
		if (A == TurnActor) continue;
		if (ProdigyAbilityUtils::IsDeadByAttributes(A)) continue;

		Target = A;
		break;
	}

	if (!IsValid(Target))
	{
		UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI turn: no valid target -> passing"));
		AdvanceTurn();
		return;
	}

	UActionComponent* AC = TurnActor->FindComponentByClass<UActionComponent>();
	if (!IsValid(AC))
	{
		UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI turn: no ActionComponent -> passing"));
		AdvanceTurn();
		return;
	}

	// Build context
	FActionContext Ctx;
	Ctx.Instigator = TurnActor;
	Ctx.TargetActor = Target;

	UWorld* World = GetWorld();
	if (!World)
	{
		const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName("Action.Attack.Basic"));
		const bool bOk = AC->ExecuteAction(AttackTag, Ctx);
		if (!bOk)
		{
			UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI action failed (no world) -> passing turn"));
			AdvanceTurn();
		}
		return;
	}

	World->GetTimerManager().ClearTimer(TimerHandle_AITakeAction);

	const float Delay = FMath::Max(0.f, AITakeActionDelaySeconds);

	const TWeakObjectPtr<UCombatSubsystem> SelfWeak(this);
	const TWeakObjectPtr<UActionComponent> ACWeak(AC);
	const TWeakObjectPtr<AActor> TurnWeak(TurnActor);
	const TWeakObjectPtr<AActor> TargetWeak(Target);
	const FActionContext LocalCtx = Ctx;

	FTimerDelegate D = FTimerDelegate::CreateLambda([SelfWeak, ACWeak, TurnWeak, TargetWeak, LocalCtx]()
	{
		if (!SelfWeak.IsValid()) return;
		if (!SelfWeak->IsInCombat()) return;

		if (!ACWeak.IsValid() || !TurnWeak.IsValid() || !TargetWeak.IsValid())
		{
			SelfWeak->AdvanceTurn();
			return;
		}

		if (SelfWeak->GetCurrentTurnActor() != TurnWeak.Get())
		{
			return;
		}

		if (ProdigyAbilityUtils::IsDeadByAttributes(TargetWeak.Get()))
		{
			UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI target died while waiting -> passing"));
			SelfWeak->AdvanceTurn();
			return;
		}

		const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName("Action.Attack.Basic"));
		const bool bOk = ACWeak->ExecuteAction(AttackTag, LocalCtx);

		if (!bOk)
		{
			UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI action failed -> passing turn (%s)"),
				*GetNameSafe(TurnWeak.Get()));
			SelfWeak->AdvanceTurn();
		}
	});

	if (Delay <= 0.f)
	{
		World->GetTimerManager().SetTimerForNextTick(D);
	}
	else
	{
		World->GetTimerManager().SetTimer(TimerHandle_AITakeAction, D, Delay, false);
		UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI scheduled: Actor=%s Delay=%.2f"),
			*GetNameSafe(TurnActor), Delay);
	}
}

AActor* UCombatSubsystem::GetCurrentTurnActor() const
{
	return Participants.IsValidIndex(TurnIndex) ? Participants[TurnIndex].Get() : nullptr;
}

void UCombatSubsystem::AdvanceTurn()
{
	if (!bInCombat) return;

	PruneParticipants();
	if (!bInCombat) return;

	// NEW: end-check after prune
	TryEndCombat_Simple();
	if (!bInCombat) return;

	if (Participants.Num() == 0) return;

	// Guard: don't schedule another begin-turn if one is already queued
	if (bTurnBeginScheduled)
	{
		UE_LOG(LogActionExec, Verbose, TEXT("[Combat] AdvanceTurn ignored (begin turn already scheduled)"));
		return;
	}

	AActor* CurrentActor = GetCurrentTurnActor();

	const int32 NextIndex = (TurnIndex + 1) % Participants.Num();

	UE_LOG(LogActionExec, Warning,
		TEXT("[Combat] AdvanceTurn: CurrentIndex=%d Current=%s NextIndex=%d Next=%s"),
		TurnIndex,
		*GetNameSafe(CurrentActor),
		NextIndex,
		*GetNameSafe(Participants.IsValidIndex(NextIndex) ? Participants[NextIndex].Get() : nullptr));

	ScheduleBeginTurnForIndex(NextIndex, BetweenTurnsDelaySeconds);
}

void UCombatSubsystem::HandleActionExecuted(FGameplayTag ActionTag, const FActionContext& Context)
{
	if (!bInCombat) return;

	AActor* Current = GetCurrentTurnActor();
	if (!IsValid(Current)) return;

	if (Context.Instigator != Current)
	{
		UE_LOG(LogActionExec, Verbose,
			   TEXT("[Combat] Ignoring ActionExecuted (not current turn): Inst=%s Current=%s"),
			   *GetNameSafe(Context.Instigator), *GetNameSafe(Current));
		return;
	}

	// Defer prune/end check by 1 tick so hit/kill cues can spawn first
	if (UWorld* World = GetWorld())
	{
		const TWeakObjectPtr<UCombatSubsystem> SelfWeak(this);
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([SelfWeak]()
		{
			if (!SelfWeak.IsValid()) return;
			if (!SelfWeak->IsInCombat()) return;
			SelfWeak->TryEndCombat_Simple();
		}));
	}

	// STYLE B RULE:
	// Do NOT AdvanceTurn here for AI. The BT will end the turn via BTT_EndCombatTurn.
	// Player also ends via UI calling EndCurrentTurn().
}

bool UCombatSubsystem::EndCurrentTurn(AActor* Instigator)
{
	if (!bInCombat) return false;
	if (!IsValid(Instigator)) return false;

	AActor* Current = GetCurrentTurnActor();
	if (Current != Instigator) return false;

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] EndCurrentTurn: %s"), *GetNameSafe(Instigator));

	// Player might have killed the last enemy and then pressed End Turn.
	// Don’t schedule next turn if combat should end.
	TryEndCombat_Simple();
	if (!bInCombat) return true;

	AdvanceTurn();
	return true;
}

void UCombatSubsystem::PruneParticipants()
{
	const int32 Before = Participants.Num();

	Participants.RemoveAll([this](const TWeakObjectPtr<AActor>& W)
	{
		AActor* A = W.Get();
		return !IsValid(A) || A->IsActorBeingDestroyed() || ProdigyAbilityUtils::IsDeadByAttributes(A);
	});

	if (Participants.Num() != Before)
	{
		OnParticipantsChanged.Broadcast();
	}

	if (Participants.Num() == 0)
	{
		// don’t auto-exit here; end condition is handled by TryEndCombat_Simple()
		return;
	}

	TurnIndex = TurnIndex % Participants.Num();
	if (PendingTurnIndex != INDEX_NONE)
	{
		PendingTurnIndex = PendingTurnIndex % Participants.Num();
	}
}

void UCombatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Warning, TEXT("[CombatSubsystem] Initialize GI=%s World=%s"),
		*GetNameSafe(GetGameInstance()),
		*GetNameSafe(GetWorld()));

	// Best-practice: bind when the actual game/PIE world is initialized
	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UCombatSubsystem::HandlePostWorldInitialization);

	// Try immediately too (sometimes world is already valid in PIE)
	if (UWorld* World = GetWorld())
	{
		TryBindToWorldEvents(World);
	}
}

void UCombatSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UWorldCombatEvents* Events = World->GetSubsystem<UWorldCombatEvents>())
		{
			Events->OnCombatStartRequested.RemoveAll(this);
			Events->OnWorldEffectApplied.RemoveAll(this);
		}
	}

	Super::Deinitialize();
}

void UCombatSubsystem::TryBindToWorldEvents(UWorld* World)
{
	if (!World) return;

	// If already bound to this world, do nothing
	if (bBoundToWorldEvents && BoundWorldEvents.IsValid() && BoundWorldEvents->GetWorld() == World)
	{
		return;
	}

	UWorldCombatEvents* Events = World->GetSubsystem<UWorldCombatEvents>();
	UE_LOG(LogTemp, Warning, TEXT("[CombatSubsystem] TryBindToWorldEvents World=%s Events=%s"),
		*GetNameSafe(World),
		*GetNameSafe(Events));

	if (!Events)
	{
		return;
	}

	// Clear any previous binding (safe even if already cleared)
	Events->OnCombatStartRequested.RemoveAll(this);
	Events->OnWorldEffectApplied.RemoveAll(this);

	Events->OnCombatStartRequested.AddDynamic(this, &UCombatSubsystem::HandleCombatStartRequested);
	Events->OnWorldEffectApplied.AddDynamic(this, &UCombatSubsystem::HandleWorldEffectApplied);

	BoundWorldEvents = Events;
	bBoundToWorldEvents = true;

	UE_LOG(LogTemp, Warning, TEXT("[CombatSubsystem] Bound to WorldCombatEvents=%s (World=%s)"),
		*GetNameSafe(Events),
		*GetNameSafe(World));
}

void UCombatSubsystem::HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
{
	(void)IVS;

	if (!World) return;

	// Only bind for the actual gameplay worlds
	const EWorldType::Type WT = World->WorldType;
	if (!(WT == EWorldType::Game || WT == EWorldType::PIE))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[CombatSubsystem] PostWorldInit World=%s Type=%d"),
		*GetNameSafe(World),
		(int32)WT);

	TryBindToWorldEvents(World);
}


void UCombatSubsystem::HandleCombatStartRequested(AActor* AggroActor, AActor* TargetActor, AActor* FirstToAct)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[CombatSubsystem] HandleCombatStartRequested Aggro=%s Target=%s First=%s InCombat=%d"),
		*GetNameSafe(AggroActor),
		*GetNameSafe(TargetActor),
		*GetNameSafe(FirstToAct),
		IsInCombat() ? 1 : 0);

	if (bInCombat)
	{
		return;
	}

	if (!IsValid(AggroActor) || !IsValid(TargetActor))
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatSubsystem] CombatStartRequested invalid args"));
		return;
	}

	// Player first (your current rule)
	AActor* PlayerPawn = FirstToAct;
	if (!IsValid(PlayerPawn))
	{
		PlayerPawn = TargetActor;
	}

	TArray<AActor*> Parts;
	BuildParticipantsForAggro(AggroActor, PlayerPawn, Parts);

	// Safety: ensure target is included
	if (IsValid(TargetActor))
	{
		Parts.AddUnique(TargetActor);
	}

	// Safety: ensure the first actor is included
	if (IsValid(PlayerPawn))
	{
		Parts.AddUnique(PlayerPawn);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[CombatSubsystem] EnterCombat Participants=%d FirstToAct=%s"),
		Parts.Num(),
		*GetNameSafe(PlayerPawn));

	EnterCombat(Parts, /*FirstToAct*/ PlayerPawn);
}

void UCombatSubsystem::HandleWorldDamageEvent(AActor* TargetActor, AActor* InstigatorActor, float AppliedDamage,
	float OldHP, float NewHP)
{
	// Only meaningful if damage actually happened
	if (!IsValid(TargetActor) || !IsValid(InstigatorActor)) return;
	if (AppliedDamage <= 0.f) return;

	// If already in combat, ignore
	if (bInCombat) return;

	// Must be valid combatants by your existing rules
	if (!IsValidCombatant(TargetActor) || !IsValidCombatant(InstigatorActor))
	{
		return;
	}

	// Player first (current rule)
	AActor* PlayerPawn = ResolveSinglePlayerPawn();
	if (!IsValid(PlayerPawn))
	{
		// Fallback: if we can’t resolve, still start, but pick target as first
		PlayerPawn = TargetActor;
	}

	// Decide which side is the "aggro source" for ally-pull:
	// If an enemy did damage, pull its allies. If player did damage, pull target enemy allies.
	AActor* AggroSource = nullptr;

	if (Cast<AEnemyCombatantCharacter>(InstigatorActor))
	{
		AggroSource = InstigatorActor;
	}
	else if (Cast<AEnemyCombatantCharacter>(TargetActor))
	{
		AggroSource = TargetActor;
	}
	else
	{
		// No enemy class involved -> 1v1 only
		AggroSource = InstigatorActor;
	}

	TArray<AActor*> Parts;
	BuildParticipantsForAggro(AggroSource, PlayerPawn, Parts);

	// Ensure the actual two involved actors are present even if AggroSource logic was weird
	Parts.AddUnique(InstigatorActor);
	Parts.AddUnique(TargetActor);

	EnterCombat(Parts, /*FirstToAct*/ PlayerPawn);
}

void UCombatSubsystem::HandleWorldEffectApplied(AActor* TargetActor, AActor* InstigatorActor, FGameplayTag ActionTag)
{
	// No re-entry
	if (bInCombat) return;

	if (!IsValid(TargetActor) || !IsValid(InstigatorActor)) return;

	// Only actions applied to an attackable enemy should start combat
	AEnemyCombatantCharacter* TargetEnemy = Cast<AEnemyCombatantCharacter>(TargetActor);
	if (!TargetEnemy) return;

	// Must be valid combatants by your subsystem rules
	if (!IsValidCombatant(TargetActor) || !IsValidCombatant(InstigatorActor))
	{
		return;
	}

	AActor* PlayerPawn = ResolveSinglePlayerPawn();
	if (!IsValid(PlayerPawn))
	{
		// Fallback: still start combat deterministically
		PlayerPawn = InstigatorActor;
	}

	// Build participants: anchor on the *enemy* so nearby enemies join
	TArray<AActor*> Parts;
	BuildParticipantsForAggro(TargetEnemy, PlayerPawn, Parts);

	// Ensure instigator/target are included (safety)
	Parts.AddUnique(InstigatorActor);
	Parts.AddUnique(TargetActor);

	// Player first (current rule)
	EnterCombat(Parts, /*FirstToAct*/ PlayerPawn);
}

bool UCombatSubsystem::AreHostile(const AActor* A, const AActor* B)
{
	if (!IsValid(A) || !IsValid(B) || A == B) return false;

	const FGameplayTag FA = GetFactionTag(A);
	const FGameplayTag FB = GetFactionTag(B);

	// If either missing, fallback to “enemy vs player” only (safe default)
	if (!FA.IsValid() || !FB.IsValid())
	{
		const bool bAIsEnemy = A->IsA(AEnemyCombatantCharacter::StaticClass());
		const bool bBIsEnemy = B->IsA(AEnemyCombatantCharacter::StaticClass());
		const bool bAIsPlayer = Cast<APawn>(A) && Cast<APawn>(A)->IsPlayerControlled();
		const bool bBIsPlayer = Cast<APawn>(B) && Cast<APawn>(B)->IsPlayerControlled();
		return (bAIsEnemy && bBIsPlayer) || (bBIsEnemy && bAIsPlayer);
	}

	// Simple rule for now: different factions are hostile
	return !FA.MatchesTagExact(FB);
}

FGameplayTag UCombatSubsystem::GetFactionTag(const AActor* Actor)
{
	if (!IsValid(Actor)) return FGameplayTag();

	// Enemies
	if (const AEnemyCombatantCharacter* E = Cast<AEnemyCombatantCharacter>(Actor))
	{
		return E->FactionTag;
	}

	// Player pawn: you can add a tag later if needed; for now treat player as "Player"
	// If you want: add FactionTag to ACombatantCharacterBase to unify.
	return FGameplayTag::RequestGameplayTag(FName("Faction.Player"));
}

bool UCombatSubsystem::IsPlayerDead() const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!IsValid(PlayerPawn)) return false;

	return ProdigyAbilityUtils::IsDeadByAttributes(PlayerPawn);
}

bool UCombatSubsystem::AreAllEnemiesDead() const
{
	bool bHasAnyEnemy = false;

	for (const TWeakObjectPtr<AActor>& W : Participants)
	{
		AActor* A = W.Get();
		if (!IsValid(A)) continue;

		AEnemyCombatantCharacter* Enemy = Cast<AEnemyCombatantCharacter>(A);
		if (!IsValid(Enemy)) continue;

		bHasAnyEnemy = true;

		if (!ProdigyAbilityUtils::IsDeadByAttributes(Enemy))
		{
			return false; // at least one enemy alive
		}
	}

	// If no enemies in participants, treat as resolved.
	return !bHasAnyEnemy;
}

void UCombatSubsystem::TryEndCombat_Simple()
{
	if (!bInCombat) return;

	PruneParticipants();

	const bool bPlayerDead = IsPlayerDead();
	const bool bEnemiesDead = AreAllEnemiesDead();

	if (bPlayerDead || bEnemiesDead)
	{
		UE_LOG(LogActionExec, Warning,
			TEXT("[CombatSubsystem] EndCombat(Simple) PlayerDead=%d EnemiesDead=%d"),
			bPlayerDead ? 1 : 0,
			bEnemiesDead ? 1 : 0);

		ExitCombat(); // your existing exit
	}
}

void UCombatSubsystem::CancelScheduledBeginTurn()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_BeginTurn);
	}
	bTurnBeginScheduled = false;
	PendingTurnIndex = INDEX_NONE;
}

void UCombatSubsystem::MarkAITurnHandled(AActor* AIActor)
{
	if (!bInCombat) return;
	if (!IsValid(AIActor)) return;

	// Only accept a mark for the current AI turn actor
	if (CurrentAITurnActor.Get() != AIActor)
	{
		return;
	}

	bCurrentAITurnHandled = true;

	UE_LOG(LogActionExec, Warning, TEXT("[CombatSubsystem] MarkAITurnHandled Actor=%s"), *GetNameSafe(AIActor));
}

void UCombatSubsystem::ScheduleBeginTurnForIndex(int32 Index, float DelaySeconds)
{
	if (!bInCombat) return;

	PruneParticipants();
	if (!bInCombat) return; // Prune may ExitCombat
	if (Participants.Num() == 0) return;

	Index = Index % Participants.Num();

	CancelScheduledBeginTurn();

	PendingTurnIndex = Index;
	bTurnBeginScheduled = true;

	UWorld* World = GetWorld();
	if (!World) return;

	const float Delay = FMath::Max(0.f, DelaySeconds);

	if (Delay <= 0.f)
	{
		HandleScheduledBeginTurn();
		return;
	}

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] ScheduleBeginTurn: Index=%d Delay=%.2f"), Index, Delay);

	World->GetTimerManager().SetTimer(
		TimerHandle_BeginTurn,
		this,
		&UCombatSubsystem::HandleScheduledBeginTurn,
		Delay,
		false
	);
}

void UCombatSubsystem::HandleScheduledBeginTurn()
{
	UE_LOG(LogActionExec, Warning, TEXT("[Combat] HandleScheduledBeginTurn: Pending=%d"), PendingTurnIndex);

	bTurnBeginScheduled = false;

	if (!bInCombat) return;

	PruneParticipants();
	if (!bInCombat) return;

	// NEW: end-check before setting TurnIndex and calling BeginTurn
	TryEndCombat_Simple();
	if (!bInCombat) return;

	if (Participants.Num() == 0) return;

	if (PendingTurnIndex == INDEX_NONE)
	{
		// Fallback – keep current
		PendingTurnIndex = TurnIndex % Participants.Num();
	}
	else
	{
		PendingTurnIndex = PendingTurnIndex % Participants.Num();
	}

	TurnIndex = PendingTurnIndex;
	PendingTurnIndex = INDEX_NONE;

	BeginTurnForIndex(TurnIndex);
}

void UCombatSubsystem::BuildParticipantsForAggro(
	AActor* AggroActor,
	AActor* PlayerPawn,
	TArray<AActor*>& OutParticipants) const
{
	OutParticipants.Reset();

	if (IsValid(PlayerPawn))
	{
		OutParticipants.AddUnique(PlayerPawn);
	}

	if (!IsValid(AggroActor))
	{
		return;
	}

	OutParticipants.AddUnique(AggroActor);

	const AEnemyCombatantCharacter* AggroEnemy = Cast<AEnemyCombatantCharacter>(AggroActor);
	if (!IsValid(AggroEnemy))
	{
		UE_LOG(LogActionExec, Warning,
			TEXT("[CombatSubsystem] BuildParticipantsForAggro: AggroActor is not AEnemyCombatantCharacter: %s"),
			*GetNameSafe(AggroActor));
		return;
	}

	const FGameplayTag AggroFaction = AggroEnemy->FactionTag;
	const float Radius = FMath::Max(0.f, AggroEnemy->AllyJoinRadius);

	if (Radius <= 0.f)
	{
		return;
	}

	UWorld* World = AggroActor->GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(Combat_AllyJoin), /*bTraceComplex*/ false);
	QueryParams.AddIgnoredActor(AggroActor);
	if (IsValid(PlayerPawn))
	{
		QueryParams.AddIgnoredActor(PlayerPawn);
	}

	const FVector Center = AggroActor->GetActorLocation();
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

	const bool bAny = World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		ObjParams,
		Shape,
		QueryParams
	);

	UE_LOG(LogActionExec, Warning,
		TEXT("[CombatSubsystem] AllyJoin overlap Aggro=%s Radius=%.1f Hits=%d AggroFaction=%s"),
		*GetNameSafe(AggroActor),
		Radius,
		bAny ? Overlaps.Num() : 0,
		AggroFaction.IsValid() ? *AggroFaction.ToString() : TEXT("None"));

	if (!bAny)
	{
		return;
	}

	for (const FOverlapResult& R : Overlaps)
	{
		AActor* Other = R.GetActor();
		if (!IsValid(Other)) continue;

		AEnemyCombatantCharacter* OtherEnemy = Cast<AEnemyCombatantCharacter>(Other);
		if (!IsValid(OtherEnemy)) continue;

		// Designer opt-out
		if (!OtherEnemy->bAggressive) continue;

		// Must be combat-ready per your rules
		if (!IsValidCombatant(OtherEnemy)) continue;

		// Don’t pull dead allies
		if (ProdigyAbilityUtils::IsDeadByAttributes(OtherEnemy)) continue;

		// ---- Faction gate ----
		if (AggroFaction.IsValid())
		{
			// Strict once faction is set
			if (!OtherEnemy->FactionTag.MatchesTagExact(AggroFaction))
			{
				continue;
			}
		}
		else
		{
			// Fallback for current “None” world:
			// Only pull exact same class as the aggro enemy.
			// (Prevents future “bandits + monsters” cross-pulls until you set faction tags.)
			if (OtherEnemy->GetClass() != AggroEnemy->GetClass())
			{
				continue;
			}
		}

		OutParticipants.AddUnique(OtherEnemy);
	}

	UE_LOG(LogActionExec, Warning,
		TEXT("[CombatSubsystem] BuildParticipantsForAggro result Participants=%d (includes Player=%s)"),
		OutParticipants.Num(),
		*GetNameSafe(PlayerPawn));
}

AActor* UCombatSubsystem::ResolveSinglePlayerPawn() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	return UGameplayStatics::GetPlayerPawn(World, 0);
}
