#include "Player/ProdigyPlayerController.h"

#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/ActionTypes.h"
#include "AbilitySystem/AttributesComponent.h"
#include "AbilitySystem/CombatSubsystem.h"
#include "AbilitySystem/EquipModSource.h"
#include "AbilitySystem/ProdigyGameplayTags.h"
#include "Quest/QuestLogComponent.h"
#include "Quest/Integration/QuestIntegrationComponent.h"
#include "GameFramework/Actor.h"
#include "Interfaces/CombatantInterface.h"
#include "Interfaces/UInv_Interactable.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipMods, Log, All);

AProdigyPlayerController::AProdigyPlayerController()
{
	// Nothing required here; components can be added in BP.
}

void AProdigyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	CacheComponents();

	BindInventoryDelegates();

	ReapplyAllEquipmentMods();

	OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePossessedPawnChanged);

	UE_LOG(LogTemp, Warning, TEXT("ProdigyPC BeginPlay: %s Local=%d Pawn=%s"),
	       *GetNameSafe(this), IsLocalController(), *GetNameSafe(GetPawn()));
}

bool AProdigyPlayerController::HandlePrimaryClickActor(AActor* ClickedActor)
{
	TARGET_LOG(Verbose,
	           TEXT("HandlePrimaryClickActor: %s"),
	           *GetNameSafe(ClickedActor)
	);

	if (TryLockTarget(ClickedActor))
	{
		TARGET_LOG(Log, TEXT("Click consumed by target lock"));
		return true;
	}

	return false;
}

void AProdigyPlayerController::CacheComponents()
{
	if (!QuestLog)
	{
		QuestLog = FindComponentByClass<UQuestLogComponent>();
	}

	if (!QuestIntegration)
	{
		QuestIntegration = FindComponentByClass<UQuestIntegrationComponent>();
	}

	if (!Inventory.IsValid())
	{
		Inventory = FindComponentByClass<UInventoryComponent>();
	}

	if (!EquipmentComp.IsValid())
	{
		EquipmentComp = FindComponentByClass<UInvEquipmentComponent>();
	}

	UE_LOG(LogEquipMods, Warning,
	       TEXT("[PC] CacheComps QuestLog=%s QuestInt=%s Inv=%s (%p) EquipComp=%s (%p)"),
	       *GetNameSafe(QuestLog),
	       *GetNameSafe(QuestIntegration),
	       *GetNameSafe(Inventory.Get()),
	       Inventory.Get(),
	       *GetNameSafe(EquipmentComp.Get()),
	       EquipmentComp.Get());
}

void AProdigyPlayerController::NotifyQuestsInventoryChanged()
{
	QuestLog->NotifyInventoryChanged();
}

void AProdigyPlayerController::NotifyQuestsKillTag(FGameplayTag TargetTag)
{
	if (TargetTag.IsValid())
	{
		QuestLog->NotifyKillObjectiveTag(TargetTag);
	}
}

void AProdigyPlayerController::ApplyQuests(FName QuestID)
{
	UE_LOG(LogTemp, Warning, TEXT("AProdigyPlayerController ApplyQuests Owner=%s"), *QuestID.ToString());
	QuestLog->AddQuest(QuestID);
}

void AProdigyPlayerController::PrimaryInteract()
{
	UE_LOG(LogTemp, Warning, TEXT("PrimaryInteract fired"));

	// Use hover (already computed by your trace timer in base)
	AActor* Target = GetHoveredActor();
	if (!IsValid(Target))
	{
		return;
	}

	// 1) If it's a valid combat target, lock it (RMB select)
	if (TryLockTarget(Target))
	{
		TARGET_LOG(Log, TEXT("PrimaryInteract consumed by target lock"));
		return;
	}

	// 2) If it's an item, pick it up (or move to it + pickup)
	if (TryPrimaryPickup(Target))
	{
		return;
	}

	// 3) Otherwise, generic interaction
	if (Target->GetClass()->ImplementsInterface(UInv_Interactable::StaticClass()))
	{
		IInv_Interactable::Execute_Interact(Target, this, GetPawn());
		return;
	}

	if (UActorComponent* InteractableComp = Target->FindComponentByInterface(UInv_Interactable::StaticClass()))
	{
		IInv_Interactable::Execute_Interact(InteractableComp, this, GetPawn());
		return;
	}
}

void AProdigyPlayerController::ClearLockedTarget()
{
	if (!LockedTarget) return;

	TARGET_LOG(Log,
	           TEXT("ClearTarget (was %s)"),
	           *GetNameSafe(LockedTarget)
	);

	LockedTarget = nullptr;
	OnTargetLocked.Broadcast(nullptr);
}

bool AProdigyPlayerController::TryLockTargetUnderCursor()
{
	FHitResult Hit;
	const bool bHit = GetHitResultUnderCursor(
		TargetTraceChannel,
		/*bTraceComplex*/ false,
		Hit
	);

	AActor* Candidate = bHit ? Hit.GetActor() : nullptr;

	TARGET_LOG(Verbose,
	           TEXT("TryLockTargetUnderCursor: Hit=%d Actor=%s"),
	           bHit ? 1 : 0,
	           *GetNameSafe(Candidate)
	);

	if (!IsValid(Candidate))
	{
		ClearLockedTarget();
		return false;
	}

	return TryLockTarget(Candidate);
}

AActor* AProdigyPlayerController::GetActorUnderCursorForClick() const
{
	FHitResult Hit;

	// 1) Combat target trace first
	const bool bHitTarget = GetHitResultUnderCursor(TargetTraceChannel, false, Hit);
	if (bHitTarget && Hit.GetActor()) return Hit.GetActor();

	// 2) Fallback to base behavior (interactables/items)
	return Super::GetActorUnderCursorForClick();
}

bool AProdigyPlayerController::TryLockTarget(AActor* Candidate)
{
	if (!IsValid(Candidate))
	{
		TARGET_LOG(Verbose, TEXT("TryLockTarget: invalid candidate"));
		return false;
	}

	if (bOnlyLockCombatants &&
		!Candidate->GetClass()->ImplementsInterface(UCombatantInterface::StaticClass()))
	{
		TARGET_LOG(Verbose,
		           TEXT("TryLockTarget: rejected %s (not Combatant)"),
		           *GetNameSafe(Candidate)
		);
		return false;
	}

	// Toggle off
	if (LockedTarget == Candidate)
	{
		TARGET_LOG(Log,
		           TEXT("TryLockTarget: toggle off %s"),
		           *GetNameSafe(Candidate)
		);
		ClearLockedTarget();
		return true;
	}

	TARGET_LOG(Log,
	           TEXT("TryLockTarget: success -> %s"),
	           *GetNameSafe(Candidate)
	);

	SetLockedTarget(Candidate);
	return true;
}


void AProdigyPlayerController::SetLockedTarget(AActor* NewTarget)
{
	if (LockedTarget == NewTarget) return;

	TARGET_LOG(Log,
	           TEXT("LockTarget -> %s"),
	           *GetNameSafe(NewTarget)
	);

	LockedTarget = NewTarget;
	OnTargetLocked.Broadcast(LockedTarget);
}

FActionContext AProdigyPlayerController::BuildActionContextFromLockedTarget() const
{
	FActionContext Ctx;
	Ctx.Instigator = GetPawn();
	Ctx.TargetActor = LockedTarget;
	return Ctx;
}

bool AProdigyPlayerController::IsMyTurn() const
{
	APawn* P = GetPawn();
	if (!IsValid(P)) return false;

	UCombatSubsystem* Combat = GetCombatSubsystem();
	if (!Combat || !Combat->IsInCombat()) return true; // not in combat => allow

	return Combat->GetCurrentTurnActor() == P;
}

bool AProdigyPlayerController::TryUseAbilityOnLockedTarget(FGameplayTag AbilityTag)
{
	APawn* P = GetPawn();
	if (!IsValid(P)) return false;

	UCombatSubsystem* Combat = GetCombatSubsystem();
	if (!Combat) return false;

	// Turn gate (only when in combat)
	if (Combat->IsInCombat())
	{
		AActor* TurnActor = Combat->GetCurrentTurnActor();
		if (TurnActor != P)
		{
			UE_LOG(LogActionExec, Warning,
			       TEXT("[PC:%s] Ability blocked: not your turn (Turn=%s)"),
			       *GetNameSafe(this), *GetNameSafe(TurnActor));
			return false;
		}
	}

	AActor* Target = LockedTarget.Get();
	if (!IsValid(Target))
	{
		UE_LOG(LogActionExec, Warning, TEXT("[PC:%s] Ability blocked: no LockedTarget"), *GetNameSafe(this));
		return false;
	}

	// Start combat only when you actually cast (not on select)
	if (!Combat->IsInCombat())
	{
		TArray<AActor*> Parts;
		Parts.Add(Target);
		Parts.Add(P);
		Combat->EnterCombat(Parts, /*FirstToAct*/ P);
	}

	UActionComponent* AC = P->FindComponentByClass<UActionComponent>();
	if (!AC) return false;

	FActionContext Ctx;
	Ctx.Instigator = P;
	Ctx.TargetActor = Target;

	return AC->ExecuteAction(AbilityTag, Ctx);
}

void AProdigyPlayerController::EndTurn()
{
	if (!IsMyTurn()) return;

	if (UCombatSubsystem* Combat = GetCombatSubsystem())
	{
		Combat->EndCurrentTurn(GetPawn());
	}
}

UCombatSubsystem* AProdigyPlayerController::GetCombatSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UCombatSubsystem>() : nullptr;
}

void AProdigyPlayerController::HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn)
{
	UE_LOG(LogEquipMods, Warning, TEXT("[PC] PawnChanged Old=%s New=%s"),
	       *GetNameSafe(PreviousPawn), *GetNameSafe(NewPawn));

	Attributes = ResolveAttributesFromPawn(NewPawn);

	// re-apply all equipped mods to new pawn
	ReapplyAllEquipmentMods();
}

UAttributesComponent* AProdigyPlayerController::ResolveAttributesFromPawn(APawn* OwnPawn) const
{
	if (!IsValid(OwnPawn)) return nullptr;
	return OwnPawn->FindComponentByClass<UAttributesComponent>();
}

void AProdigyPlayerController::BindInventoryDelegates()
{
	if (!Inventory.IsValid())
	{
		UE_LOG(LogEquipMods, Warning, TEXT("[PC] No InventoryComponent to bind"));
		return;
	}

	Inventory->OnItemEquipped.RemoveAll(this);
	Inventory->OnItemUnequipped.RemoveAll(this);

	Inventory->OnItemEquipped.AddDynamic(this, &ThisClass::HandleItemEquipped);
	Inventory->OnItemUnequipped.AddDynamic(this, &ThisClass::HandleItemUnequipped);

	UE_LOG(LogEquipMods, Warning, TEXT("[PC] Bound Inventory equip delegates Inv=%s (%p) Owner=%s"),
	       *GetNameSafe(Inventory.Get()),
	       Inventory.Get(),
	       *GetNameSafe(Inventory.IsValid() ? Inventory->GetOwner() : nullptr));
}

UObject* AProdigyPlayerController::GetOrCreateEquipSource(FGameplayTag SlotTag)
{
	if (!SlotTag.IsValid()) return nullptr;

	if (TObjectPtr<UEquipModSource>* Found = EquipSources.Find(SlotTag))
	{
		return Found->Get();
	}

	UEquipModSource* NewSource = NewObject<UEquipModSource>(this);
	NewSource->SlotTag = SlotTag;

	EquipSources.Add(SlotTag, NewSource);
	return NewSource;
}

void AProdigyPlayerController::ClearAllEquipSources(UAttributesComponent* Attr)
{
	if (!IsValid(Attr)) return;

	for (auto& Pair : EquipSources)
	{
		if (IsValid(Pair.Value))
		{
			Attr->ClearModsForSource(Pair.Value, this);
		}
	}
}

static EAttrModOp ToMainOp(EInvAttrModOp In)
{
	switch (In)
	{
	case EInvAttrModOp::Add: return EAttrModOp::Add;
	case EInvAttrModOp::Multiply: return EAttrModOp::Multiply;
	case EInvAttrModOp::Override: return EAttrModOp::Override;
	default: return EAttrModOp::Add;
	}
}


static void ConvertInvMods(const TArray<FInvAttributeMod>& In, TArray<FAttributeMod>& Out)
{
	Out.Reset();
	Out.Reserve(In.Num());

	for (const FInvAttributeMod& M : In)
	{
		if (!M.AttributeTag.IsValid())
		{
			UE_LOG(LogEquipMods, Verbose, TEXT("[ConvertInvMods] skip invalid tag"));
			continue;
		}

		FAttributeMod X;
		X.AttributeTag = M.AttributeTag;
		X.Op = ToMainOp(M.Op);
		X.Magnitude = M.Magnitude;

		UE_LOG(LogEquipMods, Warning,
		       TEXT("[ConvertInvMods] Tag=%s Op=%d Mag=%.2f"),
		       *M.AttributeTag.ToString(),
		       (int32)X.Op,
		       X.Magnitude);

		Out.Add(X);
	}
}

void AProdigyPlayerController::HandleItemEquipped(FGameplayTag EquipSlotTag, FName ItemID)
{
	if (!Inventory.IsValid()) return;

	UE_LOG(LogEquipMods, Warning, TEXT("[PC] HandleItemEquipped Slot=%s Item=%s"),
	       *EquipSlotTag.ToString(), *ItemID.ToString());

	UAttributesComponent* Attr = Attributes.Get();
	if (!IsValid(Attr))
	{
		Attr = ResolveAttributesFromPawn(GetPawn());
		Attributes = Attr;
	}
	if (!IsValid(Attr)) return;

	FItemRow Row;
	if (!Inventory->TryGetItemDef(ItemID, Row))
	{
		UE_LOG(LogEquipMods, Warning, TEXT("  -> TryGetItemDef failed for %s"), *ItemID.ToString());
		return;
	}

	TArray<FAttributeMod> Mods;
	ConvertInvMods(Row.AttributeMods, Mods);

	UObject* Source = GetOrCreateEquipSource(EquipSlotTag);
	if (!IsValid(Source)) return;

	Attr->SetModsForSource(Source, Mods, GetPawn());

	UE_LOG(LogEquipMods, Warning,
	       TEXT("[PC] After equip Slot=%s Item=%s  FinalMaxHealth=%.2f  CurHealth=%.2f  FinalHealth=%.2f"),
	       *EquipSlotTag.ToString(),
	       *ItemID.ToString(),
	       Attr->GetFinalValue(ProdigyTags::Attr::MaxHealth),
	       Attr->GetCurrentValue(ProdigyTags::Attr::Health),
	       Attr->GetFinalValue(ProdigyTags::Attr::Health));
}

void AProdigyPlayerController::HandleItemUnequipped(FGameplayTag EquipSlotTag, FName ItemID)
{
	UE_LOG(LogEquipMods, Warning, TEXT("[PC] HandleItemUnequipped Slot=%s Item=%s"),
	       *EquipSlotTag.ToString(), *ItemID.ToString());

	UAttributesComponent* Attr = Attributes.Get();
	if (!Attr)
	{
		Attr = ResolveAttributesFromPawn(GetPawn());
		Attributes = Attr;
	}
	if (!Attr) return;

	UObject* Source = GetOrCreateEquipSource(EquipSlotTag);
	if (!IsValid(Source)) return;

	Attr->ClearModsForSource(Source, /*InstigatorSource*/ GetPawn());

	UE_LOG(LogEquipMods, Warning,
	       TEXT("[PC] After unequip Slot=%s Item=%s  FinalMaxHealth=%.2f  CurHealth=%.2f  FinalHealth=%.2f"),
	       *EquipSlotTag.ToString(),
	       *ItemID.ToString(),
	       Attr->GetFinalValue(ProdigyTags::Attr::MaxHealth),
	       Attr->GetCurrentValue(ProdigyTags::Attr::Health),
	       Attr->GetFinalValue(ProdigyTags::Attr::Health));
}

bool AProdigyPlayerController::ConsumeFromSlot(int32 SlotIndex, TArray<int32>& OutChanged)
{
	OutChanged.Reset();

	// Use the existing base resolver (you already have this in AInvPlayerController)
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid()) return false;

	UInventoryComponent* Inv = InventoryComponent.Get();
	if (!IsValid(Inv)) return false;

	// Validate slot + get ItemID
	if (!Inv->IsValidIndex(SlotIndex)) return false;

	const FInventorySlot S = Inv->GetSlot(SlotIndex);
	if (S.IsEmpty()) return false;

	const FName ItemID = S.ItemID;

	// Pull item row
	FItemRow Row;
	if (!Inv->TryGetItemDef(ItemID, Row))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Consume] Missing item def ItemID=%s"), *ItemID.ToString());
		return false;
	}

	// Optional gate: require Consumable category
	if (Row.Category != EItemCategory::Consumable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Consume] Item not consumable ItemID=%s Cat=%d"),
		       *ItemID.ToString(), (int32)Row.Category);
		return false;
	}

	// Attributes live on pawn
	APawn* P = GetPawn();
	if (!IsValid(P)) return false;

	UAttributesComponent* Attr = P->FindComponentByClass<UAttributesComponent>();
	if (!IsValid(Attr))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Consume] No AttributesComponent on Pawn=%s"), *GetNameSafe(P));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Consume] Begin ItemID=%s InvMods=%d"), *ItemID.ToString(), Row.AttributeMods.Num());

	// Convert FInvAttributeMod -> FAttributeMod (required by ApplyItemAttributeModsAsCurrentDeltas)
	TArray<FAttributeMod> ItemMods;
	ConvertInvMods(Row.AttributeMods, ItemMods);

	UE_LOG(LogTemp, Warning, TEXT("[Consume] ConvertedMods=%d"), ItemMods.Num());

	// Apply item-defined attribute deltas in AttributesComponent
	const bool bApplied = Attr->ApplyItemAttributeModsAsCurrentDeltas(ItemMods, /*InstigatorActor*/ P);

	UE_LOG(LogTemp, Warning, TEXT("[Consume] Applied=%d ItemID=%s Mods=%d"),
	       bApplied ? 1 : 0, *ItemID.ToString(), ItemMods.Num());

	// Remove 1 item only if we got this far
	const bool bRemoved = Inv->RemoveFromSlot(SlotIndex, 1, OutChanged);

	UE_LOG(LogTemp, Warning, TEXT("[Consume] End ItemID=%s Removed=%d Changed=%d"),
	       *ItemID.ToString(), bRemoved ? 1 : 0, OutChanged.Num());

	return bRemoved;
}

void AProdigyPlayerController::ReapplyAllEquipmentMods()
{
	UAttributesComponent* Attr = Attributes.Get();
	if (!IsValid(Attr))
	{
		Attr = ResolveAttributesFromPawn(GetPawn());
		Attributes = Attr;
	}
	if (!IsValid(Attr)) return;

	UInventoryComponent* Inv = Inventory.Get();
	if (!IsValid(Inv))
	{
		UE_LOG(LogEquipMods, Warning, TEXT("[PC] ReapplyAllEquipmentMods: Inventory invalid"));
		return;
	}

	UE_LOG(LogEquipMods, Warning, TEXT("[PC] ReapplyAllEquipmentMods begin"));

	// 1) Clear previous equipment-derived mod sources
	ClearAllEquipSources(Attr);

	// 2) Reapply from equipped state (InventoryComponent owns EquippedItems)
	const TArray<FEquippedItemEntry> EquippedItems = Inv->GetEquippedItemsCopy();

	UE_LOG(LogEquipMods, Warning, TEXT("[PC] EquippedItems=%d"), EquippedItems.Num());

	for (const FEquippedItemEntry& E : EquippedItems)
	{
		const FGameplayTag SlotTag = E.EquipSlotTag;
		const FName ItemID = E.ItemID;

		if (!SlotTag.IsValid() || ItemID.IsNone()) continue;

		FItemRow Row;
		if (!Inv->TryGetItemDef(ItemID, Row))
		{
			UE_LOG(LogEquipMods, Verbose, TEXT("[PC]  - Missing item def ItemID=%s"), *ItemID.ToString());
			continue;
		}

		TArray<FAttributeMod> Mods;
		ConvertInvMods(Row.AttributeMods, Mods);

		UObject* Source = GetOrCreateEquipSource(SlotTag);
		if (!IsValid(Source)) continue;

		Attr->SetModsForSource(Source, Mods, this);

		UE_LOG(LogEquipMods, Verbose, TEXT("[PC]  + Slot=%s Item=%s Mods=%d"),
		       *SlotTag.ToString(), *ItemID.ToString(), Mods.Num());
	}

	UE_LOG(LogEquipMods, Warning, TEXT("[PC] ReapplyAllEquipmentMods end"));
}
