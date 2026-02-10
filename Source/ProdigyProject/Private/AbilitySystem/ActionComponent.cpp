#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/ActionAgentInterface.h"
#include "AbilitySystem/AttributesComponent.h"
#include "AbilitySystem/CombatSubsystem.h"
#include "AbilitySystem/ProdigyGameplayTags.h"

UActionComponent::UActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UActionComponent::OnRegister()
{
	Super::OnRegister();
	BuildMapIfNeeded();
}

void UActionComponent::OnTurnBegan()
{
	if (!bInCombat) return;

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)) return;

	// --- 1) Refresh AP from MaxAP (unified attributes model) ---
	if (UAttributesComponent* Attrs = OwnerActor->FindComponentByClass<UAttributesComponent>())
	{
		const FGameplayTag APTag    = ProdigyTags::Attr::AP;
		const FGameplayTag MaxAPTag = ProdigyTags::Attr::MaxAP;

		// Only do it if both exist on this actor
		if (Attrs->HasAttribute(APTag) && Attrs->HasAttribute(MaxAPTag))
		{
			const float NewAP = Attrs->GetCurrentValue(MaxAPTag);
			Attrs->SetCurrentValue(APTag, NewAP, /*InstigatorActor*/ OwnerActor);

			ACTION_LOG(Log, TEXT("OnTurnBegan: AP refreshed to %.0f (from MaxAP)"), NewAP);
		}
	}

	// --- 2) Decrement combat cooldowns at start of owner's own turn ---
	for (auto& KVP : Cooldowns)
	{
		FActionCooldownState& S = KVP.Value;
		if (S.TurnsRemaining > 0)
		{
			--S.TurnsRemaining;
		}
	}

	ACTION_LOG(Log, TEXT("OnTurnBegan: cooldown turns decremented"));
}

void UActionComponent::OnTurnEnded()
{
	ACTION_LOG(Log, TEXT("OnTurnEnded"));
}

void UActionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UActionComponent::BuildMapIfNeeded()
{
	ActionMap.Reset();
	for (UActionDefinition* Def : KnownActions)
	{
		if (!IsValid(Def)) continue;
		if (!Def->ActionTag.IsValid()) continue;
		ActionMap.Add(Def->ActionTag, Def);
	}
}

const UActionDefinition* UActionComponent::FindDef(FGameplayTag Tag) const
{
	if (const TObjectPtr<UActionDefinition>* Found = ActionMap.Find(Tag))
	{
		return Found->Get();
	}
	return nullptr;
}

void UActionComponent::SetInCombat(bool bNowInCombat)
{
	bInCombat = bNowInCombat;
}


void UActionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// No per-frame cooldown ticking anymore:
	// - Exploration cooldowns are based on CooldownEndTime vs World Time
	// - Combat cooldowns decrement on turn events (OnTurnBegan)
}


bool UActionComponent::PassesTagGates(AActor* Instigator, const UActionDefinition* Def,
                                      EActionFailReason& OutFail) const
{
	if (!IsValid(Instigator) || !Def) return false;

	FGameplayTagContainer Owned;
	if (Instigator->GetClass()->ImplementsInterface(UActionAgentInterface::StaticClass()))
	{
		IActionAgentInterface::Execute_GetOwnedGameplayTags(Instigator, Owned);
	}

	// Blocked wins
	for (const FGameplayTag& T : Def->BlockedTags)
	{
		if (Owned.HasTag(T))
		{
			OutFail = EActionFailReason::BlockedByTags;
			return false;
		}
	}

	// Required
	if (!Owned.HasAll(Def->RequiredTags))
	{
		OutFail = EActionFailReason::MissingRequiredTags;
		return false;
	}

	return true;
}

bool UActionComponent::IsTargetValid(const UActionDefinition* Def, const FActionContext& Context) const
{
	if (!Def) return false;

	switch (Def->TargetingMode)
	{
	case EActionTargetingMode::None:
		return true;
	case EActionTargetingMode::Self:
		return Context.Instigator != nullptr;
	case EActionTargetingMode::Unit:
		return Context.TargetActor != nullptr;
	case EActionTargetingMode::Point:
		return !Context.TargetLocation.IsNearlyZero(); // you can relax this later
	default:
		return false;
	}
}

FActionQueryResult UActionComponent::QueryAction(FGameplayTag ActionTag, const FActionContext& Context) const
{
	FActionQueryResult R;


	const UActionDefinition* Def = FindDef(ActionTag);
	if (!Def)
	{
		R.FailReason = EActionFailReason::NoDefinition;
		return R;
	}

	// Mode gating
	if (bInCombat && !Def->bUsableInCombat)
	{
		R.FailReason = EActionFailReason::BlockedByTags;
		return R;
	}
	if (!bInCombat && !Def->bUsableInExploration)
	{
		R.FailReason = EActionFailReason::BlockedByTags;
		return R;
	}
	// Target validity
	R.bTargetValid = IsTargetValid(Def, Context);
	if (!R.bTargetValid)
	{
		R.FailReason = EActionFailReason::InvalidTarget;
		return R;
	}

	// Tag gates
	EActionFailReason GateFail = EActionFailReason::None;
	if (!PassesTagGates(Context.Instigator, Def, GateFail))
	{
		R.FailReason = GateFail;
		return R;
	}

	// Cooldown
	if (const FActionCooldownState* S = Cooldowns.Find(ActionTag))
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

		if (S->IsOnCooldown(Now))
		{
			R.FailReason = EActionFailReason::OnCooldown;

			// UI preview
			R.CooldownTurns = S->TurnsRemaining;
			R.CooldownSeconds = S->GetSecondsRemaining(Now);

			ACTION_LOG(Log, TEXT("Query BLOCK Tag=%s Reason=Cooldown Turns=%d Sec=%.2f"),
			           *ActionTag.ToString(),
			           S->TurnsRemaining,
			           R.CooldownSeconds
			);

			return R;
		}
	}

	// Costs
	if (bInCombat)
	{
		R.APCost = Def->Combat.APCost;
		R.CooldownTurns = Def->Combat.CooldownTurns;

		if (R.APCost > 0)
		{
			if (!IsValid(Context.Instigator) || !Context.Instigator->GetClass()->ImplementsInterface(
				UActionAgentInterface::StaticClass()))
			{
				R.FailReason = EActionFailReason::InsufficientAP;
				return R;
			}

			const FGameplayTag APTag = ProdigyTags::Attr::AP;
			const float CurAP = IActionAgentInterface::Execute_GetAttributeCurrentValue(Context.Instigator, APTag);
			if (CurAP < (float)R.APCost)
			{
				R.FailReason = EActionFailReason::InsufficientAP;
				return R;
			}
		}
	}
	else
	{
		R.CooldownSeconds = Def->Exploration.CooldownSeconds;
	}

	R.bCanExecute = true;

	UE_LOG(LogActionExec, Verbose,
	       TEXT("[ActionComp:%s] Query OK Tag=%s Mode=%s APCost=%d CDTurns=%d CDSec=%.2f"),
	       *GetNameSafe(GetOwner()),
	       *ActionTag.ToString(),
	       bInCombat ? TEXT("Combat") : TEXT("Exploration"),
	       R.APCost,
	       R.CooldownTurns,
	       R.CooldownSeconds
	);

	return R;
}

void UActionComponent::StartCooldown(const UActionDefinition* Def)
{
	if (!Def) return;

	FActionCooldownState& S = Cooldowns.FindOrAdd(Def->ActionTag);

	if (bInCombat)
	{
		const int32 Planned = FMath::Max(0, Def->Combat.CooldownTurns);
		// Turn-based
		S.TurnsRemaining = (Planned > 0) ? (Planned + 1) : 0;

		// Clear realtime
		S.CooldownEndTime = 0.f;

		ACTION_LOG(Log, TEXT("StartCooldown Combat: Tag=%s Turns=%d"),
		           *Def->ActionTag.ToString(), S.TurnsRemaining);
	}
	else
	{
		// Realtime (absolute time)
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		S.CooldownEndTime = Now + FMath::Max(0.f, Def->Exploration.CooldownSeconds);

		// Clear turn-based
		S.TurnsRemaining = 0;

		ACTION_LOG(Log, TEXT("StartCooldown Explore: Tag=%s End=%.2f (Now=%.2f, Sec=%.2f)"),
		           *Def->ActionTag.ToString(), S.CooldownEndTime, Now, Def->Exploration.CooldownSeconds);
	}
}

bool UActionComponent::ExecuteAction(FGameplayTag ActionTag, const FActionContext& Context)
{
	ACTION_LOG(Log,
	           TEXT("ExecuteAction START Tag=%s Instigator=%s Target=%s InCombat=%d"),
	           *ActionTag.ToString(),
	           *GetNameSafe(Context.Instigator),
	           *GetNameSafe(Context.TargetActor),
	           bInCombat ? 1 : 0
	);


	const UActionDefinition* Def = FindDef(ActionTag);
	if (!Def)
	{
		ACTION_LOG(Warning,
		           TEXT("ExecuteAction FAIL: Definition not found for Tag=%s"),
		           *ActionTag.ToString()
		);
		return false;
	}

	const FActionQueryResult Q = QueryAction(ActionTag, Context);
	if (!Q.bCanExecute)
	{
		ACTION_LOG(Log,
		           TEXT("ExecuteAction BLOCKED:  APCost=%d"),
		           Q.APCost
		);
		
		return false;
	}

	// Spend AP only in combat
	if (bInCombat && Q.APCost > 0)
	{
		ACTION_LOG(Verbose,
		           TEXT("Attempting SpendAP=%d on %s"),
		           Q.APCost,
		           *GetNameSafe(Context.Instigator)
		);

		const FGameplayTag APTag = ProdigyTags::Attr::AP;

		const bool bSpent =
			IActionAgentInterface::Execute_ModifyAttributeCurrentValue(
				Context.Instigator,
				APTag,
				-(float)Q.APCost,
				GetOwner()
			);

		if (!bSpent)
		{
			ACTION_LOG(Warning,
					   TEXT("ExecuteAction FAIL: ModifyAttributeCurrentValue failed (Cost=%d Instigator=%s)"),
					   Q.APCost,
					   *GetNameSafe(Context.Instigator)
			);
			return false;
		}
	}

	// Apply effects
	int32 AppliedEffects = 0;
	for (const UActionEffect* E : Def->Effects)
	{
		if (!IsValid(E)) continue;

		const bool bOk = E->Apply(Context);
		AppliedEffects++;

		ACTION_LOG(Verbose,
		           TEXT("Effect Applied: %s Result=%d"),
		           *GetNameSafe(E),
		           bOk ? 1 : 0
		);
	}

	ACTION_LOG(Log,
	           TEXT("Cooldown Plan: Mode=%s Turns=%d Seconds=%.2f"),
	           bInCombat ? TEXT("Combat") : TEXT("Exploration"),
	           Q.CooldownTurns,
	           Q.CooldownSeconds
	);

	StartCooldown(Def);

	ACTION_LOG(Log,
	           TEXT("ExecuteAction SUCCESS Tag=%s Effects=%d CooldownStarted"),
	           *ActionTag.ToString(),
	           AppliedEffects
	);

	OnActionExecuted.Broadcast(ActionTag, Context);
	return true;
}
