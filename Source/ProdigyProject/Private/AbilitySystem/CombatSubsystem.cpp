
#include "AbilitySystem/CombatSubsystem.h"

#include "AbilitySystem/ActionComponent.h"

bool UCombatSubsystem::IsValidCombatant(AActor* A) const
{
	if (!IsValid(A)) return false;

	// TEMP: allow any actor with ActionComponent
	return (A->FindComponentByClass<UActionComponent>() != nullptr);
}

static void TickEndOfTurnForActor(AActor* Actor)
{
	if (!IsValid(Actor)) return;
	if (UActionComponent* AC = Actor->FindComponentByClass<UActionComponent>())
	{
		AC->OnTurnEnded();
	}
}

void UCombatSubsystem::ExitCombat()
{
	if (!bInCombat) return;

	// Unbind + reset combat flags
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

	Participants.Reset();
	TurnIndex = 0;
	bAdvancingTurn = false;
	bInCombat = false;
}

void UCombatSubsystem::EnterCombat(const TArray<AActor*>& InParticipants, AActor* FirstToAct)
{
	if (bInCombat) return;

	bInCombat = true;
	bAdvancingTurn = false;
	Participants.Reset();

	for (AActor* A : InParticipants)
	{
		if (!IsValidCombatant(A)) continue;

		Participants.Add(A);

		if (UActionComponent* AC = A->FindComponentByClass<UActionComponent>())
		{
			AC->SetInCombat(true);

			// Avoid duplicate binds
			AC->OnActionExecuted.RemoveDynamic(this, &UCombatSubsystem::HandleActionExecuted);
			AC->OnActionExecuted.AddDynamic(this, &UCombatSubsystem::HandleActionExecuted);
		}
	}

	TurnIndex = Participants.IndexOfByKey(FirstToAct);
	if (TurnIndex == INDEX_NONE)
	{
		TurnIndex = 0;
	}

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] EnterCombat: Participants=%d FirstToAct=%s TurnIndex=%d"),
	Participants.Num(), *GetNameSafe(FirstToAct), TurnIndex);

	for (int32 i = 0; i < Participants.Num(); ++i)
	{
		UE_LOG(LogActionExec, Warning, TEXT("[Combat]  P[%d]=%s Valid=%d HasActionComp=%d"),
			i,
			*GetNameSafe(Participants[i].Get()),
			IsValid(Participants[i].Get()),
			IsValid(Participants[i].Get()) && Participants[i]->FindComponentByClass<UActionComponent>() != nullptr);
	}

	BeginTurnForIndex(TurnIndex);
}

void UCombatSubsystem::BeginTurnForIndex(int32 Index)
{
	PruneParticipants();
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
		if (IsValid(A) && A != TurnActor)
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

	// Execute on next tick to avoid re-entrancy during BeginTurn / delegate callbacks
	if (UWorld* World = GetWorld())
	{
		const TWeakObjectPtr<UCombatSubsystem> SelfWeak(this);
		const TWeakObjectPtr<UActionComponent> ACWeak(AC);
		const TWeakObjectPtr<AActor> TurnWeak(TurnActor);
		const TWeakObjectPtr<AActor> TargetWeak(Target);

		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([SelfWeak, ACWeak, TurnWeak, TargetWeak, Ctx]()
		{
			if (!SelfWeak.IsValid()) return;
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
			const bool bOk = ACWeak->ExecuteAction(AttackTag, Ctx);

			// If blocked (cooldown / AP / invalid), PASS TURN so combat never stalls.
			if (!bOk)
			{
				UE_LOG(LogActionExec, Warning, TEXT("[Combat] AI action failed -> passing turn (%s)"),
					*GetNameSafe(TurnWeak.Get()));
				SelfWeak->AdvanceTurn();
			}
			// If succeeded, HandleActionExecuted will AdvanceTurn() normally.
		}));
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
	if (Participants.Num() == 0) return;

	if (bAdvancingTurn)
	{
		UE_LOG(LogActionExec, Verbose, TEXT("[Combat] AdvanceTurn ignored (already advancing)"));
		return;
	}

	bAdvancingTurn = true;

	// ✅ capture current BEFORE changing index
	AActor* CurrentActor = GetCurrentTurnActor();

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] AdvanceTurn: BEFORE TurnIndex=%d Current=%s Participants=%d"),
		TurnIndex, *GetNameSafe(CurrentActor), Participants.Num());

	// ✅ Option A: end-of-turn tick for the actor that is finishing
	TickEndOfTurnForActor(CurrentActor);

	// now pick next
	TurnIndex = (TurnIndex + 1) % Participants.Num();

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] AdvanceTurn: AFTER  TurnIndex=%d Next=%s"),
		TurnIndex, *GetNameSafe(GetCurrentTurnActor()));

	// begin next on next tick
	if (UWorld* World = GetWorld())
	{
		const int32 IndexToBegin = TurnIndex;
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UCombatSubsystem::BeginTurnForIndex, IndexToBegin)
		);
	}
	else
	{
		BeginTurnForIndex(TurnIndex);
	}

	bAdvancingTurn = false;
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

	AdvanceTurn(); 
}

bool UCombatSubsystem::EndCurrentTurn(AActor* Instigator)
{
	if (!bInCombat) return false;
	if (!IsValid(Instigator)) return false;

	AActor* Current = GetCurrentTurnActor();
	if (Current != Instigator) return false;

	UE_LOG(LogActionExec, Warning, TEXT("[Combat] EndCurrentTurn: %s"), *GetNameSafe(Instigator));

	AdvanceTurn(); // ✅ this will tick end-of-turn cooldowns for the instigator
	return true;
}

void UCombatSubsystem::PruneParticipants()
{
	Participants.RemoveAll([](const TWeakObjectPtr<AActor>& W)
	{
		AActor* A = W.Get();
		return !IsValid(A) || A->IsActorBeingDestroyed();
	});

	if (Participants.Num() == 0)
	{
		ExitCombat();
		return;
	}

	// Keep index in range
	TurnIndex = TurnIndex % Participants.Num();
}