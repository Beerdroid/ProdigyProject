#include "AbilitySystem/CombatSubsystem.h"

#include "EngineUtils.h"
#include "AbilitySystem/ActionAgentInterface.h"
#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/ActionCueSubsystem.h"
#include "AbilitySystem/ProdigyAbilityUtils.h"
#include "AbilitySystem/ProdigyGameplayTags.h"
#include "AbilitySystem/WorldCombatEvents.h"
#include "Character/EnemyCombatantCharacter.h"
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

	if (Participants.Num() == 0) return;
	if (!Participants.IsValidIndex(Index)) return;

	AActor* TurnActor = Participants[Index].Get();
	if (!IsValid(TurnActor)) return;

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] BeginTurn: Index=%d Actor=%s"),
	       Index, *GetNameSafe(TurnActor));

	// ✅ only begin-turn work now (AP refresh etc.)
	if (UActionComponent* AC = TurnActor->FindComponentByClass<UActionComponent>())
	{
		AC->OnTurnBegan();
	}

	OnTurnActorChanged.Broadcast(TurnActor);

	// Player waits
	APawn* PawnOwner = Cast<APawn>(TurnActor);
	const bool bIsPlayer = PawnOwner && PawnOwner->IsPlayerControlled();
	if (bIsPlayer)
	{
		UE_LOG(LogActionExec, Warning, TEXT("[Combat] Player turn: waiting for input"));
		return;
	}

	// ---- AI TURN ----

	// Choose target (simple 2-participant case)
	AActor* Target = nullptr;
	for (const TWeakObjectPtr<AActor>& W : Participants)
	{
		AActor* A = W.Get();
		if (IsValid(A) && A != TurnActor && !ProdigyAbilityUtils::IsDeadByAttributes(A))
		{
			Target = A;
			break;
		}
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

	// Execute after an optional AI delay (and still avoid re-entrancy)
	if (UWorld* World = GetWorld())
	{
		// Cancel any pending AI action from previous state
		World->GetTimerManager().ClearTimer(TimerHandle_AITakeAction);

		const float Delay = FMath::Max(0.f, AITakeActionDelaySeconds);

		const TWeakObjectPtr<UCombatSubsystem> SelfWeak(this);
		const TWeakObjectPtr<UActionComponent> ACWeak(AC);
		const TWeakObjectPtr<AActor> TurnWeak(TurnActor);
		const TWeakObjectPtr<AActor> TargetWeak(Target);

		// IMPORTANT: capture a *fresh* context that uses the chosen target
		FActionContext LocalCtx = Ctx;

		FTimerDelegate D = FTimerDelegate::CreateLambda([SelfWeak, ACWeak, TurnWeak, TargetWeak, LocalCtx]()
		{
			if (!SelfWeak.IsValid()) return;

			// If combat ended while waiting
			if (!SelfWeak->IsInCombat())
			{
				return;
			}

			if (!ACWeak.IsValid() || !TurnWeak.IsValid() || !TargetWeak.IsValid())
			{
				SelfWeak->AdvanceTurn();
				return;
			}

			// Still must be the current actor when we act
			if (SelfWeak->GetCurrentTurnActor() != TurnWeak.Get())
			{
				return;
			}

			const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName("Action.Attack.Basic"));
			const bool bOk = ACWeak->ExecuteAction(AttackTag, LocalCtx);

			// If blocked (cooldown / AP / invalid), PASS TURN so combat never stalls.
			if (!bOk)
			{
				UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI action failed -> passing turn (%s)"),
				       *GetNameSafe(TurnWeak.Get()));
				SelfWeak->AdvanceTurn();
			}
			// If succeeded, HandleActionExecuted will AdvanceTurn() normally.
		});

		if (Delay <= 0.f)
		{
			// still avoid re-entrancy (next tick)
			World->GetTimerManager().SetTimerForNextTick(D);
		}
		else
		{
			World->GetTimerManager().SetTimer(TimerHandle_AITakeAction, D, Delay, false);

			UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI scheduled: Actor=%s Delay=%.2f"),
			       *GetNameSafe(TurnActor), Delay);
		}
	}
	else
	{
		// Fallback: immediate execute, still pass if it fails
		const bool bOk = AC->ExecuteAction(FGameplayTag::RequestGameplayTag(FName("Action.Attack.Basic")), Ctx);
		if (!bOk)
		{
			UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI action failed (no world) -> passing turn"));
			AdvanceTurn();
		}
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

	// ✅ Defer pruning by one tick so killing-blow cues can spawn before ExitCombat clears state / gates cues.
	if (UWorld* World = GetWorld())
	{
		const TWeakObjectPtr<UCombatSubsystem> SelfWeak(this);

		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([SelfWeak]()
		{
			if (!SelfWeak.IsValid()) return;
			if (!SelfWeak->IsInCombat()) return;

			SelfWeak->PruneParticipants();
		}));
	}

	// ✅ Player can act multiple times per turn; only EndTurn should advance.
	APawn* PawnOwner = Cast<APawn>(Current);
	const bool bIsPlayer = PawnOwner && PawnOwner->IsPlayerControlled();
	if (bIsPlayer)
	{
		UE_LOG(LogActionExec, Verbose,
			   TEXT("[Combat] ActionExecuted by player: staying on same turn (wait for EndTurn)"));
		return;
	}

	// ✅ AI still advances automatically after a successful action
	AdvanceTurn();
}

bool UCombatSubsystem::EndCurrentTurn(AActor* Instigator)
{
	if (!bInCombat) return false;
	if (!IsValid(Instigator)) return false;

	AActor* Current = GetCurrentTurnActor();
	if (Current != Instigator) return false;

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] EndCurrentTurn: %s"), *GetNameSafe(Instigator));

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

	if (Participants.Num() <= 1)
	{
		ExitCombat();   // ExitCombat already broadcasts CombatStateChanged / TurnActorChanged
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

void UCombatSubsystem::CancelScheduledBeginTurn()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_BeginTurn);
	}
	bTurnBeginScheduled = false;
	PendingTurnIndex = INDEX_NONE;
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

void UCombatSubsystem::BuildParticipantsForAggro(AActor* AggroActor, AActor* PlayerPawn,
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

	// ✅ reduce junk hits: ignore player too (no behavior change, only less work)
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
		TEXT("[CombatSubsystem] AllyJoin overlap Aggro=%s Radius=%.1f Hits=%d"),
		*GetNameSafe(AggroActor),
		Radius,
		bAny ? Overlaps.Num() : 0);

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

		// Designers can opt-out per enemy
		if (!OtherEnemy->bAggressive) continue;

		// Must be a valid combatant per your existing rules
		if (!IsValidCombatant(OtherEnemy)) continue;

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
