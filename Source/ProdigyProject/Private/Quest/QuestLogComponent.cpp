#include "Quest/QuestLogComponent.h"

#include "Game/ProdigyGameState.h"
#include "GameFramework/Character.h"
#include "Quest/Interfaces/QuestInventoryProvider.h"
#include "Quest/Interfaces/QuestKillEventSource.h"
#include "Quest/Interfaces/QuestRewardReceiver.h"


class AProdigyGameState;

UQuestLogComponent::UQuestLogComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	QuestStates.Owner = this;
}



void UQuestLogComponent::BeginPlay()
{
	Super::BeginPlay();
	QuestStates.Owner = this;

	BindIntegration();

	if (UWorld* World = GetWorld())
	{
		if (AProdigyGameState* GS = World->GetGameState<AProdigyGameState>())
		{
			QuestDatabase = GS->GetQuestDatabase();
		}
	}
}

void UQuestLogComponent::HandleQuestStatesReplicated()
{
	// This is the client-side reaction to replication deltas (and safe for standalone too).
	OnQuestsUpdated.Broadcast();
}

void UQuestLogComponent::NotifyQuestStatesChanged_LocalAuthority()
{
	// Mirrors inventory: listen server / standalone need explicit local UI notification.
	const ENetMode NetMode = GetNetMode();
	if (NetMode == NM_ListenServer || NetMode == NM_Standalone)
	{
		HandleQuestStatesReplicated();
	}
}

void UQuestLogComponent::HandleKillTagNative(FGameplayTag TargetTag)
{
	NotifyKillObjectiveTag(TargetTag);
}

void UQuestLogComponent::BindIntegration()
{
	if (bIntegrationBound &&
		InventoryProvider.GetObject() && RewardReceiver.GetObject())
	{
		// Ensure delegates are still bound (kill + inventory) and return.
		if (KillEventSource.GetObject())
		{
			FOnQuestKillTagNative& KillDelegate = KillEventSource.GetInterface()->GetKillDelegate();
			KillDelegate.RemoveAll(this);
			KillDelegate.AddUObject(this, &UQuestLogComponent::HandleKillTagNative);
		}

		// Inventory delta binding
		if (InventoryProvider.GetObject())
		{
			FOnQuestInventoryDelta& InvDelta = InventoryProvider.GetInterface()->GetInventoryDeltaDelegate();
			InvDelta.RemoveAll(this);
			InvDelta.AddDynamic(this, &UQuestLogComponent::HandleInventoryDelta);
		}

		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// Reset (optional but recommended if you want hot-rebind)
	InventoryProvider = nullptr;
	RewardReceiver = nullptr;
	KillEventSource = nullptr;
	CachedIntegrationComponent = nullptr;

	// Find any component that implements our interfaces
	TArray<UActorComponent*> Components;
	OwnerActor->GetComponents(Components);

	for (UActorComponent* C : Components)
	{
		if (!C) continue;

		if (!InventoryProvider && C->GetClass()->ImplementsInterface(UQuestInventoryProvider::StaticClass()))
		{
			InventoryProvider.SetObject(C);
			InventoryProvider.SetInterface(Cast<IQuestInventoryProvider>(C));
			CachedIntegrationComponent = C;
		}

		if (!RewardReceiver && C->GetClass()->ImplementsInterface(UQuestRewardReceiver::StaticClass()))
		{
			RewardReceiver.SetObject(C);
			RewardReceiver.SetInterface(Cast<IQuestRewardReceiver>(C));
			CachedIntegrationComponent = C;
		}

		if (!KillEventSource && C->GetClass()->ImplementsInterface(UQuestKillEventSource::StaticClass()))
		{
			KillEventSource.SetObject(C);
			KillEventSource.SetInterface(Cast<IQuestKillEventSource>(C));
			CachedIntegrationComponent = C;
		}
	}

	// Bind kill delegate once (optional integration)
	if (KillEventSource)
	{
		FOnQuestKillTagNative& KillDelegate = KillEventSource.GetInterface()->GetKillDelegate();

		KillDelegate.RemoveAll(this);
		KillDelegate.AddUObject(this, &UQuestLogComponent::HandleKillTagNative);
	}

	if (InventoryProvider)
	{
		FOnQuestInventoryDelta& InvDelta = InventoryProvider.GetInterface()->GetInventoryDeltaDelegate();

		InvDelta.RemoveAll(this);
		InvDelta.AddDynamic(this, &UQuestLogComponent::HandleInventoryDelta);
	}

	bIntegrationBound = true;
}

const IQuestInventoryProvider* UQuestLogComponent::GetInventoryProviderChecked() const
{
	check(InventoryProvider);
	return InventoryProvider.GetInterface();
}

IQuestInventoryProvider* UQuestLogComponent::GetInventoryProviderCheckedMutable() const
{
	check(InventoryProvider);
	return const_cast<IQuestInventoryProvider*>(InventoryProvider.GetInterface());
}

IQuestRewardReceiver* UQuestLogComponent::GetRewardReceiverChecked() const
{
	check(RewardReceiver);
	return RewardReceiver.GetInterface();
}

void UQuestLogComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQuestLogComponent, QuestStates);
}



void UQuestLogComponent::OnRep_QuestStates()
{
	HandleQuestStatesReplicated();
}

AActor* UQuestLogComponent::GetOwnerActorChecked() const
{
	AActor* OwnerActor = GetOwner();
	check(OwnerActor);
	return OwnerActor;
}


FQuestRuntimeState* UQuestLogComponent::FindQuestStateMutable(FName QuestID)
{
	for (FQuestRuntimeState& S : QuestStates.Items)
	{
		if (S.QuestID == QuestID)
		{
			return &S;
		}
	}
	return nullptr;
}

const FQuestRuntimeState* UQuestLogComponent::FindQuestStateConst(FName QuestID) const
{
	for (const FQuestRuntimeState& S : QuestStates.Items)
	{
		if (S.QuestID == QuestID)
		{
			return &S;
		}
	}
	return nullptr;
}

int32 UQuestLogComponent::GetObjectiveProgress(const FQuestRuntimeState& State, FName ObjectiveID) const
{
	for (const FQuestObjectiveProgress& P : State.ObjectiveProgress)
	{
		if (P.ObjectiveID == ObjectiveID)
		{
			return P.Progress;
		}
	}
	return 0;
}

void UQuestLogComponent::SetObjectiveProgress(FQuestRuntimeState& State, FName ObjectiveID, int32 NewValue)
{
	NewValue = FMath::Max(0, NewValue);

	for (FQuestObjectiveProgress& P : State.ObjectiveProgress)
	{
		if (P.ObjectiveID == ObjectiveID)
		{
			const int32 OldValue = P.Progress;
			P.Progress = NewValue;

			// Mark dirty for replication
			QuestStates.MarkItemDirty(State);

			// Client-facing messages are best sent via replicated state + UI comparison,
			// but if you want immediate local feedback on owning client,
			// you can also do Client RPC here. Keeping it simple: broadcast on server won't reach client.
			// So leave messaging to UI comparing OnRep_QuestStates deltas,
			// or add owning-client RPC if you want (not included).

			(void)OldValue;
			return;
		}
	}

	// If missing, add it
	FQuestObjectiveProgress NewP;
	NewP.ObjectiveID = ObjectiveID;
	NewP.Progress = NewValue;
	State.ObjectiveProgress.Add(NewP);
	QuestStates.MarkItemDirty(State);
}

// -------------------- Server API --------------------



void UQuestLogComponent::ServerTrackQuest_Implementation(FName QuestID, bool bTrack)
{
	FQuestRuntimeState* State = FindQuestStateMutable(QuestID);
	if (!State)
	{
		return;
	}

	State->bIsTracked = bTrack;
	QuestStates.MarkItemDirty(*State);
}

void UQuestLogComponent::ServerAbandonQuest_Implementation(FName QuestID)
{
	for (int32 i = 0; i < QuestStates.Items.Num(); ++i)
	{
		if (QuestStates.Items[i].QuestID == QuestID)
		{
			QuestStates.Items.RemoveAt(i);
			QuestStates.MarkArrayDirty();
			return;
		}
	}
}

void UQuestLogComponent::InitializeStageProgress(FQuestRuntimeState& State, const FQuestDefinition& Def)
{
	State.ObjectiveProgress.Empty();

	const FQuestStageDef* Stage = GetStageDef(Def, State.CurrentStage);
	if (!Stage)
	{
		return;
	}

	for (const FQuestObjectiveDef& Obj : Stage->Objectives)
	{
		FQuestObjectiveProgress P;
		P.ObjectiveID = Obj.ObjectiveID;
		P.Progress = 0;
		State.ObjectiveProgress.Add(P);
	}

	QuestStates.MarkItemDirty(State);
	RecomputeCollectObjectivesForStage(State, Def);
}

bool UQuestLogComponent::AreAllObjectivesComplete(const FQuestRuntimeState& State, const FQuestDefinition& Def) const
{
	if (!Def.Stages.IsValidIndex(State.CurrentStage))
	{
		return false;
	}
	
	const FQuestStageDef* Stage = GetStageDef(Def, State.CurrentStage);
	if (!Stage) return false;

	for (const FQuestObjectiveDef& Obj : Stage->Objectives)
	{
		const int32 Current = GetObjectiveProgress(State, Obj.ObjectiveID);
		if (Current < Obj.RequiredQuantity)
		{
			return false;
		}
	}
	return true;
}

void UQuestLogComponent::RecomputeCollectObjectivesForStage(FQuestRuntimeState& State, const FQuestDefinition& Def)
{
	// Need inventory integration
	if (!InventoryProvider || !InventoryProvider.GetObject())
	{
		return;
	}

	const FQuestStageDef* Stage = GetStageDef(Def, State.CurrentStage);
	if (!Stage)
	{
		return;
	}

	UObject* InvObj = InventoryProvider.GetObject();

	for (const FQuestObjectiveDef& Obj : Stage->Objectives)
	{
		if (Obj.Type != EQuestObjectiveType::Collect)
		{
			continue;
		}

		if (Obj.ItemID.IsNone())
		{
			continue;
		}

		// "Current possession" semantics: progress tracks current item count.
		const int32 CurrentCount = IQuestInventoryProvider::Execute_GetTotalQuantityByItemID(InvObj, Obj.ItemID);

		// Clamp to required, so objective doesn't show 37/5 unless you want that.
		const int32 NewProgress = FMath::Clamp(CurrentCount, 0, Obj.RequiredQuantity);

		const int32 OldProgress = GetObjectiveProgress(State, Obj.ObjectiveID);
		if (OldProgress != NewProgress)
		{
			SetObjectiveProgress(State, Obj.ObjectiveID, NewProgress);
		}
	}
}

void UQuestLogComponent::ApplyKillProgress(FQuestRuntimeState& State, const FQuestDefinition& Def,
	const FGameplayTag& TargetTag, int32 Delta)
{
	const FQuestStageDef* Stage = GetStageDef(Def, State.CurrentStage);
	if (!Stage)
	{
		return;
	}

	for (const FQuestObjectiveDef& Obj : Stage->Objectives)
	{
		if (Obj.Type != EQuestObjectiveType::Kill)
		{
			continue;
		}

		// Match tags (you can choose MatchesTagExact or MatchesTag depending on your tagging scheme)
		if (TargetTag.IsValid() && Obj.TargetTag.IsValid() && TargetTag.MatchesTag(Obj.TargetTag))
		{
			const int32 Old = GetObjectiveProgress(State, Obj.ObjectiveID);
			const int32 NewVal = Old + Delta;
			SetObjectiveProgress(State, Obj.ObjectiveID, NewVal);

			// Optional immediate server-side messaging hooks (client should react on OnRep)
			if (Old < Obj.RequiredQuantity && NewVal >= Obj.RequiredQuantity)
			{
				// OnObjectiveCompletedMessage.Broadcast(Obj.ObjectiveID); // server-only
			}
		}
	}
}

void UQuestLogComponent::ServerAddQuest_Implementation(FName QuestID)
{
	if (QuestID.IsNone())
	{
		return;
	}

	// Already have it?
	if (FindQuestStateConst(QuestID))
	{
		return;
	}

	FQuestDefinition Def;
	if (!TryGetQuestDef(QuestID, Def))
	{
		UE_LOG(LogTemp, Warning, TEXT("ServerAddQuest: QuestDefinition not found for %s"), *QuestID.ToString());
		return;
	}

	// Add runtime state
	FQuestRuntimeState NewState;
	NewState.QuestID = QuestID;
	NewState.CurrentStage = 0;
	NewState.bIsTracked = false;
	NewState.bIsCompleted = false;
	NewState.bIsTurnedIn = false;

	QuestStates.Items.Add(NewState);
	QuestStates.MarkArrayDirty();

	// Initialize objectives for stage 0
	FQuestRuntimeState* State = FindQuestStateMutable(QuestID);
	if (State)
	{
		InitializeStageProgress(*State, Def);
		QuestStates.MarkItemDirty(*State);
	}
}

void UQuestLogComponent::NotifyKillObjectiveTag(FGameplayTag TargetTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!TargetTag.IsValid())
	{
		return;
	}

	BindIntegration();

	bool bAnyChanged = false;

	for (FQuestRuntimeState& State : QuestStates.Items)
	{
		if (State.bIsCompleted) continue;

		FQuestDefinition Def;
		if (!TryGetQuestDef(State.QuestID, Def)) continue;

		const int32 BeforeStage = State.CurrentStage;
		const bool bBeforeCompleted = State.bIsCompleted;
		const bool bBeforeTurnedIn = State.bIsTurnedIn;

		ApplyKillProgress(State, Def, TargetTag, 1);
		TryAdvanceStageOrComplete(State, Def);

		if (State.CurrentStage != BeforeStage ||
			State.bIsCompleted != bBeforeCompleted ||
			State.bIsTurnedIn != bBeforeTurnedIn)
		{
			bAnyChanged = true;
		}
	}

	if (bAnyChanged)
	{
		NotifyQuestStatesChanged_LocalAuthority();
	}
}

void UQuestLogComponent::NotifyInventoryChanged()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	BindIntegration();

	bool bAnyChanged = false;

	for (FQuestRuntimeState& State : QuestStates.Items)
	{
		if (State.bIsCompleted) continue;

		FQuestDefinition Def;
		if (!TryGetQuestDef(State.QuestID, Def)) continue;

		const int32 BeforeStage = State.CurrentStage;
		const bool bBeforeCompleted = State.bIsCompleted;

		RecomputeCollectObjectivesForStage(State, Def);
		TryAdvanceStageOrComplete(State, Def);

		if (State.CurrentStage != BeforeStage || State.bIsCompleted != bBeforeCompleted)
		{
			bAnyChanged = true;
		}
	}

	if (bAnyChanged)
	{
		NotifyQuestStatesChanged_LocalAuthority();
	}
}

void UQuestLogComponent::TryAdvanceStageOrComplete(FQuestRuntimeState& State, const FQuestDefinition& Def)
{
	if (State.bIsCompleted)
	{
		return;
	}

	if (!AreAllObjectivesComplete(State, Def))
	{
		return;
	}

	GrantStageRewards(Def, State.CurrentStage);

	State.CurrentStage++;

	if (Def.Stages.IsValidIndex(State.CurrentStage))
	{
		InitializeStageProgress(State, Def);
		QuestStates.MarkItemDirty(State);
		return;
	}

	State.bIsCompleted = true;
	QuestStates.MarkItemDirty(State);

	// Do NOT grant final rewards here. Completion != turned-in.
	if (Def.bAutoComplete)
	{
		// Auto-complete means: immediately turn-in on server.
		ServerTurnInQuest_Implementation(State.QuestID);
	}
}

void UQuestLogComponent::GrantStageRewards(const FQuestDefinition& Def, int32 StageIndex)
{
	if (!Def.Stages.IsValidIndex(StageIndex))
	{
		return;
	}

	const FQuestStageDef& Stage = Def.Stages[StageIndex];

	// Items via inventory provider
	if (InventoryProvider)
	{
		for (const FQuestItemReward& R : Stage.ItemRewards)
		{
			if (R.ItemID.IsNone() || R.Quantity <= 0) continue;

			IQuestInventoryProvider::Execute_AddItemByID(
				InventoryProvider.GetObject(),
				R.ItemID,
				R.Quantity,
				this
			);
		}
	}

	// Currency + XP via reward receiver
	if (RewardReceiver)
	{
		if (Stage.CurrencyReward > 0)
		{
			IQuestRewardReceiver::Execute_AddCurrency(RewardReceiver.GetObject(), Stage.CurrencyReward);
		}
		if (Stage.XPReward > 0)
		{
			IQuestRewardReceiver::Execute_AddXP(RewardReceiver.GetObject(), Stage.XPReward);
		}
	}
}

void UQuestLogComponent::GrantFinalRewards(const FQuestDefinition& Def)
{
	if (InventoryProvider)
	{
		UObject* InvObj = InventoryProvider.GetObject();

		for (const FQuestItemReward& R : Def.FinalItemRewards)
		{
			if (R.ItemID.IsNone() || R.Quantity <= 0) continue;

			IQuestInventoryProvider::Execute_AddItemByID(
				InvObj,
				R.ItemID,
				R.Quantity,
				this
			);
		}
	}

	if (RewardReceiver)
	{
		UObject* RewardObj = RewardReceiver.GetObject();

		if (Def.FinalCurrencyReward > 0)
		{
			IQuestRewardReceiver::Execute_AddCurrency(RewardObj, Def.FinalCurrencyReward);
		}
		if (Def.FinalXPReward > 0)
		{
			IQuestRewardReceiver::Execute_AddXP(RewardObj, Def.FinalXPReward);
		}
	}
}


bool UQuestLogComponent::TryGetQuestDef(FName QuestID, FQuestDefinition& OutDef) const
{
	if (!QuestDatabase || QuestID.IsNone())
	{
		return false;
	}
	return QuestDatabase->TryGetQuest(QuestID, OutDef);
}

const FQuestStageDef* UQuestLogComponent::GetStageDef(const FQuestDefinition& Def, int32 StageIndex) const
{
	if (!Def.Stages.IsValidIndex(StageIndex))
	{
		return nullptr;
	}
	return &Def.Stages[StageIndex];
}

void UQuestLogComponent::HandleInventoryDelta(const FInventoryDelta& Delta)
{
	// Inventory is authoritative on server; only recompute there.
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (Delta.ItemID.IsNone())
	{
		return;
	}

	BindIntegration();

	bool bAnyChanged = false;

	for (FQuestRuntimeState& State : QuestStates.Items)
	{
		if (State.bIsCompleted)
		{
			continue;
		}

		FQuestDefinition Def;
		if (!TryGetQuestDef(State.QuestID, Def))
		{
			continue;
		}

		const int32 BeforeStage = State.CurrentStage;
		const bool bBeforeCompleted = State.bIsCompleted;

		// Only recompute collect objectives potentially affected by this item.
		RecomputeCollectObjectivesForStage_SingleItem(State, Def, Delta.ItemID);

		// Completion may change as a result
		TryAdvanceStageOrComplete(State, Def);

		if (State.CurrentStage != BeforeStage || State.bIsCompleted != bBeforeCompleted)
		{
			bAnyChanged = true;
		}
	}

	if (bAnyChanged)
	{
		NotifyQuestStatesChanged_LocalAuthority();
	}
}

void UQuestLogComponent::RecomputeCollectObjectivesForStage_SingleItem(FQuestRuntimeState& State,
	const FQuestDefinition& Def, FName ChangedItemID)
{
	if (!InventoryProvider || !InventoryProvider.GetObject())
	{
		return;
	}

	const FQuestStageDef* Stage = GetStageDef(Def, State.CurrentStage);
	if (!Stage)
	{
		return;
	}

	UObject* InvObj = InventoryProvider.GetObject();

	for (const FQuestObjectiveDef& Obj : Stage->Objectives)
	{
		if (Obj.Type != EQuestObjectiveType::Collect)
		{
			continue;
		}

		if (Obj.ItemID != ChangedItemID)
		{
			continue;
		}

		const int32 CurrentCount = IQuestInventoryProvider::Execute_GetTotalQuantityByItemID(InvObj, Obj.ItemID);
		const int32 NewProgress = FMath::Clamp(CurrentCount, 0, Obj.RequiredQuantity);

		if (GetObjectiveProgress(State, Obj.ObjectiveID) != NewProgress)
		{
			SetObjectiveProgress(State, Obj.ObjectiveID, NewProgress);
		}
	}
}

TArray<FQuestLogEntryView> UQuestLogComponent::GetQuestLogEntries() const
{
	TArray<FQuestLogEntryView> Out;

	// Optional: if DB wasn't ready at BeginPlay on client, try to re-fetch once here.
	if (!QuestDatabase)
	{
		if (UWorld* World = GetWorld())
		{
			if (AProdigyGameState* GS = World->GetGameState<AProdigyGameState>())
			{
				const_cast<UQuestLogComponent*>(this)->QuestDatabase = GS->GetQuestDatabase();
			}
		}
	}

	for (const FQuestRuntimeState& State : QuestStates.Items)
	{
		FQuestLogEntryView V;

		V.QuestID = State.QuestID;

		// If it's in the quest log, it's accepted by definition.
		V.bIsAccepted = true;

		V.CurrentStage = State.CurrentStage;
		V.bIsTracked = State.bIsTracked;
		V.bIsCompleted = State.bIsCompleted;
		V.bIsTurnedIn = State.bIsTurnedIn;

		V.CurrentObjectiveProgress = State.ObjectiveProgress;

		FQuestDefinition Def;
		if (TryGetQuestDef(State.QuestID, Def))
		{
			V.Title = Def.Title;
			V.Description = Def.Description;

			if (const FQuestStageDef* Stage = GetStageDef(Def, State.CurrentStage))
			{
				V.CurrentObjectives = Stage->Objectives;
			}
		}

		Out.Add(MoveTemp(V));
	}

	return Out;
}

TArray<FQuestLogEntryView> UQuestLogComponent::BuildQuestOfferViews(const TArray<FName>& QuestIDs) const
{
		TArray<FQuestLogEntryView> Out;
	Out.Reserve(QuestIDs.Num());

	// If DB wasn't ready at BeginPlay on client, try to re-fetch once here.
	if (!QuestDatabase)
	{
		if (UWorld* World = GetWorld())
		{
			if (AProdigyGameState* GS = World->GetGameState<AProdigyGameState>())
			{
				const_cast<UQuestLogComponent*>(this)->QuestDatabase = GS->GetQuestDatabase();
			}
		}
	}

	for (const FName QuestID : QuestIDs)
	{
		if (QuestID.IsNone())
		{
			continue;
		}

		FQuestLogEntryView V;
		V.QuestID = QuestID;

		// --- Definition ---
		FQuestDefinition Def;
		const bool bHasDef = TryGetQuestDef(QuestID, Def);
		if (bHasDef)
		{
			V.Title = Def.Title;
			V.Description = Def.Description;
		}

		// --- Runtime (if accepted) ---
		const FQuestRuntimeState* State = FindQuestStateConst(QuestID);
		if (State)
		{
			V.bIsAccepted = true;

			V.CurrentStage = State->CurrentStage;
			V.bIsTracked  = State->bIsTracked;
			V.bIsCompleted = State->bIsCompleted;
			V.bIsTurnedIn  = State->bIsTurnedIn;

			// progress is only meaningful if accepted
			V.CurrentObjectiveProgress = State->ObjectiveProgress;

			// objectives for current stage (requires definition)
			if (bHasDef)
			{
				if (const FQuestStageDef* Stage = GetStageDef(Def, State->CurrentStage))
				{
					V.CurrentObjectives = Stage->Objectives;
				}
			}
		}
		else
		{
			// Not accepted: provide a preview (stage 0)
			V.bIsAccepted = false;

			V.CurrentStage = 0;
			V.bIsTracked = false;
			V.bIsCompleted = false;
			V.bIsTurnedIn = false;

			if (bHasDef)
			{
				if (const FQuestStageDef* Stage0 = GetStageDef(Def, 0))
				{
					V.CurrentObjectives = Stage0->Objectives;

					// Optional but recommended: pre-fill progress rows with 0
					// so the widget can render "0 / Required" uniformly.
					V.CurrentObjectiveProgress.Reset(Stage0->Objectives.Num());
					for (const FQuestObjectiveDef& Obj : Stage0->Objectives)
					{
						FQuestObjectiveProgress P;
						P.ObjectiveID = Obj.ObjectiveID;
						P.Progress = 0;
						V.CurrentObjectiveProgress.Add(P);
					}
				}
			}
		}

		Out.Add(MoveTemp(V));
	}

	return Out;
}

void UQuestLogComponent::ServerTurnInQuest_Implementation(FName QuestID)
{
	FQuestRuntimeState* State = FindQuestStateMutable(QuestID);
	if (!State)
	{
		return;
	}

	// Already turned in
	if (State->bIsTurnedIn)
	{
		return;
	}

	FQuestDefinition Def;
	if (!TryGetQuestDef(State->QuestID, Def))
	{
		return;
	}

	// Require completion
	if (!State->bIsCompleted)
	{
		return;
	}

	// OPTIONAL: remove "collect" items here if you want a true turn-in sink.
	// Requires RemoveItemByID on the provider.
	
	if (InventoryProvider)
	{
		UObject* InvObj = InventoryProvider.GetObject();
		for (const FQuestStageDef& Stage : Def.Stages)
		{
			for (const FQuestObjectiveDef& Obj : Stage.Objectives)
			{
				if (Obj.Type == EQuestObjectiveType::Collect && !Obj.ItemID.IsNone())
				{
					IQuestInventoryProvider::Execute_RemoveItemByID(InvObj, Obj.ItemID, Obj.RequiredQuantity, this);
				}
			}
		}
	}

	// Grant final rewards ONLY on turn-in
	GrantFinalRewards(Def);

	State->bIsTurnedIn = true;
	QuestStates.MarkItemDirty(*State);

	// This name is currently misleading; it's really "turned in".
	OnQuestCompleted.Broadcast(QuestID);

	NotifyQuestStatesChanged_LocalAuthority();
}
