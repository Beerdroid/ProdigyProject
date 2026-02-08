#include "Quest/QuestLogComponent.h"

#include "Game/ProdigyGameState.h"
#include "GameFramework/Character.h"
#include "Quest/Integration/QuestIntegrationComponent.h"
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
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	// Resolve PC
	APlayerController* PC = Cast<APlayerController>(OwnerActor);
	if (!PC)
	{
		if (APawn* Pawn = Cast<APawn>(OwnerActor))
		{
			PC = Cast<APlayerController>(Pawn->GetController());
		}
	}

	UQuestIntegrationComponent* NewIntegration = PC ? PC->FindComponentByClass<UQuestIntegrationComponent>() : nullptr;

	// 1) Unbind from old cached integration (IMPORTANT: do this BEFORE clearing!)
	if (IsValid(CachedIntegrationComponent))
	{
		CachedIntegrationComponent->OnQuestItemChanged.RemoveAll(this);

		// If you bind kill delegate through the same integration object, also remove here
		// (Only if you previously bound it to this)
		if (IQuestKillEventSource* KillIface = Cast<IQuestKillEventSource>(CachedIntegrationComponent))
		{
			FOnQuestKillTagNative& KillDelegate = KillIface->GetKillDelegate();
			KillDelegate.RemoveAll(this);
		}
	}

	// 2) Clear script interfaces properly
	InventoryProvider.SetObject(nullptr);
	InventoryProvider.SetInterface(nullptr);

	RewardReceiver.SetObject(nullptr);
	RewardReceiver.SetInterface(nullptr);

	KillEventSource.SetObject(nullptr);
	KillEventSource.SetInterface(nullptr);

	CachedIntegrationComponent = nullptr;
	bIntegrationBound = false;

	// 3) If no new integration, we’re done
	if (!IsValid(NewIntegration))
	{
		return;
	}

	// 4) Set providers to the new integration (adapter)
	InventoryProvider.SetObject(NewIntegration);
	InventoryProvider.SetInterface(Cast<IQuestInventoryProvider>(NewIntegration));

	RewardReceiver.SetObject(NewIntegration);
	RewardReceiver.SetInterface(Cast<IQuestRewardReceiver>(NewIntegration));

	KillEventSource.SetObject(NewIntegration);
	KillEventSource.SetInterface(Cast<IQuestKillEventSource>(NewIntegration));

	CachedIntegrationComponent = NewIntegration;

	// 5) Bind simplified delegate
	NewIntegration->OnQuestItemChanged.RemoveAll(this);
	NewIntegration->OnQuestItemChanged.AddDynamic(this, &UQuestLogComponent::HandleQuestItemChanged);

	// 6) Bind kill delegate (native)
	if (IQuestKillEventSource* KillIface = Cast<IQuestKillEventSource>(NewIntegration))
	{
		FOnQuestKillTagNative& KillDelegate = KillIface->GetKillDelegate();
		KillDelegate.RemoveAll(this);
		KillDelegate.AddUObject(this, &UQuestLogComponent::HandleKillTagNative);
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

void UQuestLogComponent::ApplyInteractProgress(FQuestRuntimeState& State, const FQuestDefinition& Def,
	const FGameplayTag& TargetTag, int32 Delta)
{
	if (!TargetTag.IsValid() || Delta == 0) return;

	for (const FQuestObjectiveDef& Obj : Def.Objectives)
	{
		if (Obj.Type != EQuestObjectiveType::Interact && Obj.Type != EQuestObjectiveType::Talk)
		{
			continue;
		}

		// Match semantic tag (you can switch to MatchesTagExact if you want strict)
		if (Obj.TargetTag.IsValid() && TargetTag.MatchesTag(Obj.TargetTag))
		{
			const int32 Old = GetObjectiveProgress(State, Obj.ObjectiveID);
			SetObjectiveProgress(State, Obj.ObjectiveID, Old + Delta);
		}
	}
}

void UQuestLogComponent::ApplyLocationProgress(FQuestRuntimeState& State, const FQuestDefinition& Def,
	const FGameplayTag& TargetTag, int32 Delta)
{
	if (!TargetTag.IsValid() || Delta == 0) return;

	for (const FQuestObjectiveDef& Obj : Def.Objectives)
	{
		if (Obj.Type != EQuestObjectiveType::LocationVisit)
		{
			continue;
		}

		if (Obj.TargetTag.IsValid() && TargetTag.MatchesTag(Obj.TargetTag))
		{
			const int32 Old = GetObjectiveProgress(State, Obj.ObjectiveID);
			SetObjectiveProgress(State, Obj.ObjectiveID, Old + Delta);
		}
	}
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

bool UQuestLogComponent::SetObjectiveProgress(FQuestRuntimeState& State, FName ObjectiveID, int32 NewValue)
{
	NewValue = FMath::Max(0, NewValue);

	for (FQuestObjectiveProgress& P : State.ObjectiveProgress)
	{
		if (P.ObjectiveID == ObjectiveID)
		{
			if (P.Progress == NewValue)
			{
				return false; // no change
			}

			P.Progress = NewValue;
			QuestStates.MarkItemDirty(State);
			return true;
		}
	}

	// If missing, add it
	FQuestObjectiveProgress NewP;
	NewP.ObjectiveID = ObjectiveID;
	NewP.Progress = NewValue;

	State.ObjectiveProgress.Add(NewP);
	QuestStates.MarkItemDirty(State);
	return true;
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

bool UQuestLogComponent::AreAllObjectivesComplete(const FQuestRuntimeState& State, const FQuestDefinition& Def) const
{
	// Important: do NOT auto-complete a quest with zero objectives
	if (Def.Objectives.Num() == 0)
	{
		return false;
	}

	for (const FQuestObjectiveDef& Obj : Def.Objectives)
	{
		// If required is <= 0, treat as already satisfied (optional policy)
		if (Obj.RequiredQuantity <= 0)
		{
			continue;
		}

		const int32 Current = GetObjectiveProgress(State, Obj.ObjectiveID);
		if (Current < Obj.RequiredQuantity)
		{
			return false;
		}
	}

	return true;
}

bool UQuestLogComponent::RecomputeCollectObjectives(FQuestRuntimeState& State, const FQuestDefinition& Def)
{
	if (!InventoryProvider || !InventoryProvider.GetObject())
	{
		return false;
	}

	bool bChanged = false;
	UObject* InvObj = InventoryProvider.GetObject();

	for (const FQuestObjectiveDef& Obj : Def.Objectives)
	{
		if (Obj.Type != EQuestObjectiveType::Collect) continue;
		if (Obj.ItemID.IsNone()) continue;

		const int32 CurrentCount = IQuestInventoryProvider::Execute_GetTotalQuantityByItemID(InvObj, Obj.ItemID);
		const int32 NewProgress = FMath::Clamp(CurrentCount, 0, Obj.RequiredQuantity);

		bChanged |= SetObjectiveProgress(State, Obj.ObjectiveID, NewProgress);
	}

	return bChanged;
}

void UQuestLogComponent::ApplyKillProgress(FQuestRuntimeState& State, const FQuestDefinition& Def,
	const FGameplayTag& TargetTag, int32 Delta)
{
	for (const FQuestObjectiveDef& Obj : Def.Objectives)
	{
		if (Obj.Type != EQuestObjectiveType::Kill)
		{
			continue;
		}

		if (TargetTag.IsValid() && Obj.TargetTag.IsValid() && TargetTag.MatchesTag(Obj.TargetTag))
		{
			const int32 Old = GetObjectiveProgress(State, Obj.ObjectiveID);
			const int32 NewVal = Old + Delta;
			SetObjectiveProgress(State, Obj.ObjectiveID, NewVal);
		}
	}
}

void UQuestLogComponent::TryCompleteQuest(FQuestRuntimeState& State, const FQuestDefinition& Def)
{
	UE_LOG(LogTemp, Warning, TEXT("TryCompleteQuest Quest=%s Completed=%d Auto=%d"),
	*State.QuestID.ToString(), State.bIsCompleted, Def.bAutoComplete);
	
	if (State.bIsCompleted)
	{
		return;
	}

	if (!AreAllObjectivesComplete(State, Def))
	{
		return;
	}

	State.bIsCompleted = true;
	QuestStates.MarkItemDirty(State);

	if (Def.bAutoComplete)
	{
		ServerTurnInQuest_Implementation(State.QuestID);
	}
}

void UQuestLogComponent::ServerAddQuest_Implementation(FName QuestID)
{
	if (QuestID.IsNone()) return;

	if (FindQuestStateConst(QuestID)) return;

	FQuestDefinition Def;
	if (!TryGetQuestDef(QuestID, Def))
	{
		UE_LOG(LogTemp, Warning, TEXT("ServerAddQuest: QuestDefinition not found for %s"), *QuestID.ToString());
		return;
	}

	FQuestRuntimeState NewState;
	NewState.QuestID = QuestID;
	NewState.bIsTracked = false;
	NewState.bIsCompleted = false;
	NewState.bIsTurnedIn = false;

	QuestStates.Items.Add(NewState);
	QuestStates.MarkArrayDirty();

	if (FQuestRuntimeState* State = FindQuestStateMutable(QuestID))
	{
		InitializeQuestProgress(*State, Def);
		TryCompleteQuest(*State, Def); // important if already has items
		QuestStates.MarkItemDirty(*State);
	}

	NotifyQuestStatesChanged_LocalAuthority();
}

void UQuestLogComponent::NotifyKillObjectiveTag(FGameplayTag TargetTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!TargetTag.IsValid()) return;

	BindIntegration();

	bool bAnyChanged = false;

	for (FQuestRuntimeState& State : QuestStates.Items)
	{
		if (State.bIsCompleted) continue;

		FQuestDefinition Def;
		if (!TryGetQuestDef(State.QuestID, Def)) continue;

		const bool bBeforeCompleted = State.bIsCompleted;

		ApplyKillProgress(State, Def, TargetTag, 1);
		TryCompleteQuest(State, Def);

		if (State.bIsCompleted != bBeforeCompleted)
		{
			bAnyChanged = true;
		}
	}

	if (bAnyChanged)
	{
		NotifyQuestStatesChanged_LocalAuthority();
	}
}

bool UQuestLogComponent::IsQuestAccepted(FName QuestID) const
{
	if (QuestID.IsNone())
	{
		return false;
	}
	return FindQuestStateConst(QuestID) != nullptr;
}

bool UQuestLogComponent::IsQuestCompleted(FName QuestID) const
{
	if (QuestID.IsNone())
	{
		return false;
	}
	if (const FQuestRuntimeState* S = FindQuestStateConst(QuestID))
	{
		return S->bIsCompleted;
	}
	return false;
}

bool UQuestLogComponent::IsQuestTurnedIn(FName QuestID) const
{
	if (const FQuestRuntimeState* S = FindQuestStateConst(QuestID))
	{
		return S->bIsTurnedIn;
	}
	return false;
}

void UQuestLogComponent::InitializeQuestProgress(FQuestRuntimeState& State, const FQuestDefinition& Def)
{
	State.ObjectiveProgress.Empty();
	State.ObjectiveProgress.Reserve(Def.Objectives.Num());

	for (const FQuestObjectiveDef& Obj : Def.Objectives)
	{
		FQuestObjectiveProgress P;
		P.ObjectiveID = Obj.ObjectiveID;
		P.Progress = 0;
		State.ObjectiveProgress.Add(P);
	}

	QuestStates.MarkItemDirty(State);

	// Precompute collect objectives from inventory possession
	RecomputeCollectObjectives(State, Def);
}

void UQuestLogComponent::NotifyInventoryChanged()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	BindIntegration();

	bool bAnyChanged = false;

	for (FQuestRuntimeState& State : QuestStates.Items)
	{
		if (State.bIsTurnedIn) continue;

		FQuestDefinition Def;
		if (!TryGetQuestDef(State.QuestID, Def)) continue;

		const bool bBeforeCompleted = State.bIsCompleted;

		RecomputeCollectObjectives(State, Def);
		TryCompleteQuest(State, Def);

		if (State.bIsCompleted != bBeforeCompleted)
		{
			bAnyChanged = true;
		}
	}

	if (bAnyChanged)
	{
		NotifyQuestStatesChanged_LocalAuthority();
	}
}

void UQuestLogComponent::GrantFinalRewards(const FQuestDefinition& Def)
{
	if (UObject* InvObj = InventoryProvider.GetObject())
	{
		for (const FQuestItemReward& R : Def.FinalItemRewards)
		{
			if (R.ItemID.IsNone() || R.Quantity <= 0) continue;

			const bool bOk = IQuestInventoryProvider::Execute_AddItemByID(InvObj, R.ItemID, R.Quantity, this);
			UE_LOG(LogTemp, Warning, TEXT("GrantFinalRewards AddItem %s x%d -> %d"),
				*R.ItemID.ToString(), R.Quantity, (int32)bOk);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GrantFinalRewards: InventoryProvider=None"));
	}

	UE_LOG(LogTemp, Warning, TEXT("GrantFinalRewards Currency=%d XP=%d"), Def.FinalCurrencyReward, Def.FinalXPReward);

	if (UObject* RewardObj = RewardReceiver.GetObject())
	{
		if (Def.FinalCurrencyReward > 0)
		{
			IQuestRewardReceiver::Execute_AddCurrency(RewardObj, Def.FinalCurrencyReward);
		}
		if (Def.FinalXPReward > 0)
		{
			IQuestRewardReceiver::Execute_AddXP(RewardObj, Def.FinalXPReward);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GrantFinalRewards: RewardReceiver=None"));
	}
}

void UQuestLogComponent::NotifyInteractTargetTag(FGameplayTag TargetTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!TargetTag.IsValid()) return;

	BindIntegration();

	bool bAnyChanged = false;

	for (FQuestRuntimeState& State : QuestStates.Items)
	{
		if (State.bIsTurnedIn) continue;

		FQuestDefinition Def;
		if (!TryGetQuestDef(State.QuestID, Def)) continue;

		const bool bBeforeCompleted = State.bIsCompleted;

		ApplyInteractProgress(State, Def, TargetTag, 1);
		TryCompleteQuest(State, Def);

		if (State.bIsCompleted != bBeforeCompleted)
		{
			bAnyChanged = true;
		}
	}

	if (bAnyChanged)
	{
		NotifyQuestStatesChanged_LocalAuthority();
	}
}

void UQuestLogComponent::NotifyLocationVisitedTag(FGameplayTag TargetTag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (!TargetTag.IsValid()) return;

	BindIntegration();

	bool bAnyChanged = false;

	for (FQuestRuntimeState& State : QuestStates.Items)
	{
		if (State.bIsTurnedIn) continue;

		FQuestDefinition Def;
		if (!TryGetQuestDef(State.QuestID, Def)) continue;

		const bool bBeforeCompleted = State.bIsCompleted;

		ApplyLocationProgress(State, Def, TargetTag, 1);
		TryCompleteQuest(State, Def);

		if (State.bIsCompleted != bBeforeCompleted)
		{
			bAnyChanged = true;
		}
	}

	if (bAnyChanged)
	{
		NotifyQuestStatesChanged_LocalAuthority();
	}
}

void UQuestLogComponent::ServerNotifyObjectiveEvent_Implementation(FName QuestID, FName ObjectiveID, UObject* Context)
{
	// Server only
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	// Validate inputs
	if (QuestID.IsNone() || ObjectiveID.IsNone()) return;

	// Must have quest
	FQuestRuntimeState* State = FindQuestStateMutable(QuestID);
	if (!State) return;                 // quest not accepted
	if (State->bIsTurnedIn) return;

	// Load quest definition
	FQuestDefinition Def;
	if (!TryGetQuestDef(QuestID, Def)) return;

	// Don't advance already completed quests (policy; you can allow if you want)
	if (State->bIsCompleted)
	{
		return;
	}

	// Find objective in DB
	const FQuestObjectiveDef* ObjDef = Def.Objectives.FindByPredicate([&](const FQuestObjectiveDef& O)
	{
		return O.ObjectiveID == ObjectiveID;
	});
	if (!ObjDef) return;

	// Optional: ignore collect objectives here (they are inventory-driven)
	// If you want to allow "Interact to Collect" objectives, remove this guard.
	if (ObjDef->Type == EQuestObjectiveType::Collect)
	{
		return;
	}

	// Delta is always +1, clamp to required quantity from DB
	const int32 Required = FMath::Max(0, ObjDef->RequiredQuantity);
	const int32 Old = GetObjectiveProgress(*State, ObjectiveID);

	int32 NewVal = Old + 1;
	if (Required > 0)
	{
		NewVal = FMath::Clamp(NewVal, 0, Required);
	}
	else
	{
		NewVal = FMath::Max(0, NewVal);
	}

	const bool bProgressChanged = SetObjectiveProgress(*State, ObjectiveID, NewVal);

	// Completion check
	const bool bBeforeCompleted = State->bIsCompleted;
	TryCompleteQuest(*State, Def);

	// Rep + UI notify (listen server / standalone path already handled in your helper)
	if (bProgressChanged || (State->bIsCompleted != bBeforeCompleted))
	{
		NotifyQuestStatesChanged_LocalAuthority();
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


void UQuestLogComponent::HandleQuestItemChanged(FName ItemID)
{
	if (ItemID.IsNone()) return;
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	bool bAnyChanged = false;
	for (FQuestRuntimeState& State : QuestStates.Items)
	{
		if (State.bIsTurnedIn) continue;

		FQuestDefinition Def;
		if (!TryGetQuestDef(State.QuestID, Def)) continue;

		const bool bBeforeCompleted = State.bIsCompleted;
		const bool bProgressChanged = RecomputeCollectObjectives_SingleItem(State, Def, ItemID);
		TryCompleteQuest(State, Def);

		bAnyChanged |= bProgressChanged || (State.bIsCompleted != bBeforeCompleted);
	}

	if (bAnyChanged)
	{
		NotifyQuestStatesChanged_LocalAuthority();
	}
}

bool UQuestLogComponent::RecomputeCollectObjectives_SingleItem(FQuestRuntimeState& State, const FQuestDefinition& Def,
                                                               FName ChangedItemID)
{
	UE_LOG(LogTemp, Warning, TEXT("RecomputeSingleItem ENTER: providerObj=%s"), *GetNameSafe(InventoryProvider.GetObject()));
	if (!InventoryProvider || !InventoryProvider.GetObject())
	{
		return false;
	}

	bool bChanged = false;
	UObject* InvObj = InventoryProvider.GetObject();

	UE_LOG(LogTemp, Warning, TEXT("RecomputeSingleItem: Quest=%s ObjCount=%d Changed=%s"),
		*State.QuestID.ToString(), Def.Objectives.Num(), *ChangedItemID.ToString());

	for (const FQuestObjectiveDef& Obj : Def.Objectives)
	{
		UE_LOG(LogTemp, Warning, TEXT("  ObjID=%s Type=%d ItemID=%s Req=%d"),
			*Obj.ObjectiveID.ToString(), (int32)Obj.Type, *Obj.ItemID.ToString(), Obj.RequiredQuantity);

		if (Obj.Type != EQuestObjectiveType::Collect) continue;
		if (Obj.ItemID != ChangedItemID) continue;

		const int32 CurrentCount = IQuestInventoryProvider::Execute_GetTotalQuantityByItemID(InvObj, Obj.ItemID);
		const int32 NewProgress = FMath::Clamp(CurrentCount, 0, Obj.RequiredQuantity);

		UE_LOG(LogTemp, Warning, TEXT("  MATCH -> invCount=%d clamped=%d"), CurrentCount, NewProgress);

		bChanged |= SetObjectiveProgress(State, Obj.ObjectiveID, NewProgress);
	}

	return bChanged;
}

TArray<FQuestLogEntryView> UQuestLogComponent::GetQuestLogEntries() const
{
	TArray<FQuestLogEntryView> Out;

	for (const FQuestRuntimeState& State : QuestStates.Items)
	{
		FQuestLogEntryView V;
		V.QuestID = State.QuestID;
		V.bIsAccepted = true;
		V.bIsTracked = State.bIsTracked;
		V.bIsCompleted = State.bIsCompleted;
		V.bIsTurnedIn = State.bIsTurnedIn;
		V.ObjectiveProgress = State.ObjectiveProgress;

		FQuestDefinition Def;
		if (TryGetQuestDef(State.QuestID, Def))
		{
			V.Title = Def.Title;
			V.Description = Def.Description;
			V.Objectives = Def.Objectives;

			V.FinalItemRewards = Def.FinalItemRewards;
			V.FinalCurrencyReward = Def.FinalCurrencyReward;
			V.FinalXPReward = Def.FinalXPReward;
		}

		Out.Add(MoveTemp(V));
	}

	return Out;
}

TArray<FQuestLogEntryView> UQuestLogComponent::BuildQuestOfferViews(const TArray<FName>& QuestIDs) const
{
	TArray<FQuestLogEntryView> Out;
	Out.Reserve(QuestIDs.Num());

	for (FName QuestID : QuestIDs)
	{
		if (QuestID.IsNone()) continue;

		FQuestDefinition Def;
		const bool bHasDef = TryGetQuestDef(QuestID, Def);
		if (!bHasDef) continue;

		FQuestLogEntryView V;
		V.QuestID = QuestID;
		V.Title = Def.Title;
		V.Description = Def.Description;
		V.Objectives = Def.Objectives;

		const FQuestRuntimeState* State = FindQuestStateConst(QuestID);
		if (State)
		{
			V.bIsAccepted = true;
			V.bIsTracked = State->bIsTracked;
			V.bIsCompleted = State->bIsCompleted;
			V.bIsTurnedIn = State->bIsTurnedIn;
			V.ObjectiveProgress = State->ObjectiveProgress;
		}
		else
		{
			V.bIsAccepted = false;
			V.bIsTracked = false;
			V.bIsCompleted = false;
			V.bIsTurnedIn = false;

			V.ObjectiveProgress.Reset(Def.Objectives.Num());
			for (const FQuestObjectiveDef& Obj : Def.Objectives)
			{
				FQuestObjectiveProgress P;
				P.ObjectiveID = Obj.ObjectiveID;
				P.Progress = 0;
				V.ObjectiveProgress.Add(P);
			}
		}

		Out.Add(MoveTemp(V));
	}

	return Out;
}

bool UQuestLogComponent::GetQuestEntryView(FName QuestID, bool& bOutIsAccepted, FQuestLogEntryView& OutEntry) const
{
	OutEntry = FQuestLogEntryView();
	bOutIsAccepted = false;

	if (QuestID.IsNone())
	{
		return false;
	}

	FQuestDefinition Def;
	if (!TryGetQuestDef(QuestID, Def))
	{
		return false;
	}

	OutEntry.QuestID = QuestID;
	OutEntry.Title = Def.Title;
	OutEntry.Description = Def.Description;
	OutEntry.Objectives = Def.Objectives;

	if (const FQuestRuntimeState* State = FindQuestStateConst(QuestID))
	{
		bOutIsAccepted = true;
		OutEntry.bIsAccepted = true;
		OutEntry.bIsTracked = State->bIsTracked;
		OutEntry.bIsCompleted = State->bIsCompleted;
		OutEntry.bIsTurnedIn = State->bIsTurnedIn;
		OutEntry.ObjectiveProgress = State->ObjectiveProgress;
	}
	else
	{
		OutEntry.bIsAccepted = false;
		OutEntry.bIsTracked = false;
		OutEntry.bIsCompleted = false;
		OutEntry.bIsTurnedIn = false;

		OutEntry.ObjectiveProgress.Reset(Def.Objectives.Num());
		for (const FQuestObjectiveDef& Obj : Def.Objectives)
		{
			FQuestObjectiveProgress P;
			P.ObjectiveID = Obj.ObjectiveID;
			P.Progress = 0;
			OutEntry.ObjectiveProgress.Add(P);
		}
	}

	return true;
}


void UQuestLogComponent::ServerTurnInQuest_Implementation(FName QuestID)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;

	FQuestRuntimeState* State = FindQuestStateMutable(QuestID);
	if (!State) return;
	if (State->bIsTurnedIn) return;

	FQuestDefinition Def;
	if (!TryGetQuestDef(State->QuestID, Def)) return;

	BindIntegration();

	UE_LOG(LogTemp, Warning, TEXT("TurnInQuest ENTER Quest=%s Completed=%d TurnedIn=%d InvProv=%s RewardRecv=%s"),
		*QuestID.ToString(),
		State->bIsCompleted,
		State->bIsTurnedIn,
		*GetNameSafe(InventoryProvider.GetObject()),
		*GetNameSafe(RewardReceiver.GetObject()));

	// Recompute collect objectives on-demand to avoid relying on delta timing.
	if (InventoryProvider && InventoryProvider.GetObject())
	{
		RecomputeCollectObjectives(*State, Def);
	}

	// Evaluate completion here (manual turn-in should work even if TryCompleteQuest wasn't triggered yet)
	if (!AreAllObjectivesComplete(*State, Def))
	{
		UE_LOG(LogTemp, Warning, TEXT("TurnInQuest FAIL Quest=%s: objectives not complete"), *QuestID.ToString());
		return;
	}

	// Mark complete (for UI correctness / replication)
	if (!State->bIsCompleted)
	{
		State->bIsCompleted = true;
	}

	// Consume collect items
	if (InventoryProvider && InventoryProvider.GetObject())
	{
		UObject* InvObj = InventoryProvider.GetObject();

		for (const FQuestObjectiveDef& Obj : Def.Objectives)
		{
			if (Obj.Type != EQuestObjectiveType::Collect) continue;
			if (Obj.ItemID.IsNone() || Obj.RequiredQuantity <= 0) continue;

			UE_LOG(LogTemp, Warning, TEXT("TurnInQuest REMOVE Item=%s Qty=%d"), *Obj.ItemID.ToString(), Obj.RequiredQuantity);

			IQuestInventoryProvider::Execute_RemoveItemByID(InvObj, Obj.ItemID, Obj.RequiredQuantity, this);
		}
	}

	// Grant rewards
	UE_LOG(LogTemp, Warning, TEXT("TurnInQuest GRANT rewards Quest=%s"), *QuestID.ToString());
	GrantFinalRewards(Def);

	// Finalize
	State->bIsTurnedIn = true;
	QuestStates.MarkItemDirty(*State);

	OnQuestCompleted.Broadcast(QuestID);
	NotifyQuestStatesChanged_LocalAuthority();
}
