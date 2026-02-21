#include "Player/ProdigyPlayerController.h"

#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/ActionTypes.h"
#include "AbilitySystem/AttributesComponent.h"
#include "AbilitySystem/CombatSubsystem.h"
#include "AbilitySystem/EquipModSource.h"
#include "AbilitySystem/ProdigyAbilityUtils.h"
#include "AbilitySystem/ProdigyGameplayTags.h"
#include "AbilitySystem/WorldCombatEvents.h"
#include "Blueprint/UserWidget.h"
#include "Character/Components/DamageTextComponent.h"
#include "Character/Components/HealthBarWidgetComponent.h"
#include "Quest/QuestLogComponent.h"
#include "Quest/Integration/QuestIntegrationComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Interfaces/CombatantInterface.h"
#include "Interfaces/UInv_Interactable.h"
#include "UI/Utils/ProdigyWidgetPlacement.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipMods, Log, All);


static void BindCombatSubsystemEvents(AProdigyPlayerController* PC)
{
	if (!PC) return;

	if (UCombatSubsystem* Combat = PC->GetCombatSubsystem())
	{
		Combat->OnCombatStateChanged.RemoveAll(PC);
		Combat->OnTurnActorChanged.RemoveAll(PC);
		Combat->OnParticipantsChanged.RemoveAll(PC);

		Combat->OnCombatStateChanged.AddDynamic(PC, &AProdigyPlayerController::HandleCombatStateChanged_FromSubsystem);
		Combat->OnTurnActorChanged.AddDynamic(PC, &AProdigyPlayerController::HandleTurnActorChanged_FromSubsystem);
		Combat->OnParticipantsChanged.AddDynamic(PC, &AProdigyPlayerController::HandleParticipantsChanged_FromSubsystem);
	}
}

AProdigyPlayerController::AProdigyPlayerController()
{
	// Nothing required here; components can be added in BP.
}

void AProdigyPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (UAttributesComponent* Attr = InPawn->FindComponentByClass<UAttributesComponent>())
	{
		Attr->OnAttributeChanged.AddDynamic(this, &AProdigyPlayerController::HandleAttrChanged_ForHUD);
	}
}


void AProdigyPlayerController::HandleAttrChanged_ForHUD(FGameplayTag Tag, float NewValue, float Delta, AActor* InInstigator)
{
	// Only broadcast for the tags HUD cares about
	if (Tag == ProdigyTags::Attr::Health || Tag == ProdigyTags::Attr::AP ||
		Tag == ProdigyTags::Attr::MaxHealth || Tag == ProdigyTags::Attr::MaxAP)
	{
		OnCombatHUDDirty.Broadcast();
	}
}


void AProdigyPlayerController::HandleCombatStateChanged_ForHUD(bool bNowInCombat)
{
	OnCombatHUDDirty.Broadcast();
}


void AProdigyPlayerController::HandleTurnChanged_ForHUD()
{
	OnCombatHUDDirty.Broadcast();
}

void AProdigyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	CacheComponents();

	BindInventoryDelegates();

	ReapplyAllEquipmentMods();

	BindCombatSubsystemEvents(this);

	OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePossessedPawnChanged);

	if (UWorld* World = GetWorld())
	{
		if (UWorldCombatEvents* Events = World->GetSubsystem<UWorldCombatEvents>())
		{
			Events->OnWorldDamageEvent.RemoveAll(this);
			Events->OnWorldHealEvent.RemoveAll(this);

			Events->OnWorldDamageEvent.AddDynamic(this, &ThisClass::HandleWorldDamageEvent);
			Events->OnWorldHealEvent.AddDynamic(this, &ThisClass::HandleWorldHealEvent);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("ProdigyPC BeginPlay: %s Local=%d Pawn=%s"),
		   *GetNameSafe(this), IsLocalController(), *GetNameSafe(GetPawn()));

	// ✅ Ensure hotbar is visible at runtime
	ShowHotbar();
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

void AProdigyPlayerController::HandleWorldDamageEvent(
	AActor* TargetActor,
	AActor* InstigatorActor,
	float AppliedDamage,
	float OldHP,
	float NewHP)
{
	if (!IsValid(TargetActor)) return;
	if (AppliedDamage <= 0.f) return;

	ACharacter* TargetChar = Cast<ACharacter>(TargetActor);
	if (!IsValid(TargetChar)) return;

	// Convention: positive = damage
	ShowDamageNumber(AppliedDamage, TargetChar);
}

void AProdigyPlayerController::HandleWorldHealEvent(
	AActor* TargetActor,
	AActor* InstigatorActor,
	float AppliedHeal,
	float OldHP,
	float NewHP)
{
	if (!IsValid(TargetActor)) return;
	if (AppliedHeal <= 0.f) return;

	ACharacter* TargetChar = Cast<ACharacter>(TargetActor);
	if (!IsValid(TargetChar)) return;

	// Convention: negative = heal (so ShowDamageNumber can route to SetHealText)
	ShowDamageNumber(-AppliedHeal, TargetChar);
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

void AProdigyPlayerController::SetParticipantsWorldHealthBarsVisible(bool bVisible)
{
	UCombatSubsystem* Combat = GetCombatSubsystem();
	if (!Combat) return;

	const TArray<TWeakObjectPtr<AActor>> Parts = Combat->GetParticipants();

	for (const TWeakObjectPtr<AActor>& W : Parts)
	{
		AActor* A = W.Get();
		if (!IsValid(A)) continue;

		APawn* P = Cast<APawn>(A);
		const bool bIsPlayerControlled = P && P->IsPlayerControlled();
		if (bIsPlayerControlled) continue;

		UHealthBarWidgetComponent* HC = A->FindComponentByClass<UHealthBarWidgetComponent>();
		if (!IsValid(HC))
		{
			UE_LOG(LogTemp, Error, TEXT("[WorldHP] MISSING on Enemy=%s Class=%s (no UHealthBarWidgetComponent found)"),
				*GetNameSafe(A),
				*GetNameSafe(A->GetClass()));
			continue;
		}

		// HC->SetVisible(bVisible);
	}
}

void AProdigyPlayerController::ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter)
{
	if (!IsValid(TargetCharacter)) return;
	if (!DamageTextComponentClass) return;

	// 0 is noise
	if (FMath::IsNearlyZero(DamageAmount)) return;

	UDamageTextComponent* DamageText =
		NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);

	if (!IsValid(DamageText)) return;

	USceneComponent* Root = TargetCharacter->GetRootComponent();
	if (!IsValid(Root)) return;

	DamageText->RegisterComponent();
	DamageText->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);

	// Small deterministic-enough jitter (single player) to avoid stacking texts perfectly
	const float Side   = FMath::FRandRange(-25.f, 25.f);
	const float Forward= FMath::FRandRange(-10.f, 10.f);
	const float Up     = FMath::FRandRange(-8.f, 8.f);

	const FVector WorldLoc = TargetCharacter->GetActorLocation() + FVector(Forward, Side, 130.f + Up);
	DamageText->SetWorldLocation(WorldLoc);

	DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	// SIGN POLICY:
	//  +X = damage
	//  -X = heal
	if (DamageAmount > 0.f)
	{
		DamageText->SetDamageText(DamageAmount);
	}
	else
	{
		DamageText->SetHealText(FMath::Abs(DamageAmount));
	}

	// Ensure we don't leak components if widget BP doesn't destroy itself
	constexpr float FloatingTextLifeSeconds = 1.5f;
	if (UWorld* World = TargetCharacter->GetWorld())
	{
		FTimerHandle Th;
		World->GetTimerManager().SetTimer(
			Th,
			[DamageText]()
			{
				if (IsValid(DamageText))
				{
					DamageText->DestroyComponent();
				}
			},
			FloatingTextLifeSeconds,
			false
		);
	}
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

	// Same target clicked again => keep lock (do NOT toggle off)
	if (LockedTarget == Candidate)
	{
		TARGET_LOG(Verbose, TEXT("TryLockTarget: same target clicked again, keep lock -> %s"),
			*GetNameSafe(Candidate));
		return true; // consume click, keep current lock
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

	TARGET_LOG(Log, TEXT("LockTarget -> %s"), *GetNameSafe(NewTarget));

	LockedTarget = NewTarget;
	OnTargetLocked.Broadcast(LockedTarget);

	ValidateCombatHUDVisibility();
	OnCombatHUDDirty.Broadcast();
}

void AProdigyPlayerController::ClearLockedTarget()
{
	if (!LockedTarget) return;

	TARGET_LOG(Log, TEXT("ClearTarget (was %s)"), *GetNameSafe(LockedTarget));

	LockedTarget = nullptr;
	OnTargetLocked.Broadcast(nullptr);

	ValidateCombatHUDVisibility();
	OnCombatHUDDirty.Broadcast();;
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

	const bool bOk = AC->ExecuteAction(AbilityTag, Ctx);

	// ✅ Force HUD refresh immediately so cooldown disables right after usage
	if (bOk)
	{
		OnCombatHUDDirty.Broadcast();
	}

	return bOk;
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

	OnCombatHUDDirty.Broadcast();

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

	OnCombatHUDDirty.Broadcast();

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

	for (const FInvPeriodicMod& PM : Row.PeriodicMods)
	{
		if (!PM.EffectTag.IsValid() || !PM.AttributeTag.IsValid()) continue;
		if (PM.NumTurns <= 0 || FMath::IsNearlyZero(PM.DeltaPerTurn)) continue;

		Attr->AddOrRefreshTurnEffect(
			PM.EffectTag,
			PM.AttributeTag,
			PM.DeltaPerTurn,
			PM.NumTurns,
			/*Instigator*/ P,
			/*Refresh*/ true,
			/*Stack*/ false
		);
	}

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

	OnCombatHUDDirty.Broadcast();
	
	UE_LOG(LogEquipMods, Warning, TEXT("[PC] ReapplyAllEquipmentMods end"));
}

void AProdigyPlayerController::ShowCombatHUD()
{
	if (!CombatHUDClass) return;

	// Create only (placement is centralized in ValidateCombatHUDVisibility -> EnsureHUD)
	if (!IsValid(CombatHUD))
	{
		CombatHUD = CreateWidget<UUserWidget>(this, CombatHUDClass);
		if (!IsValid(CombatHUD)) return;

		CombatHUD->AddToViewport(2000);

		// Start hidden; ValidateCombatHUDVisibility will decide + place
		CombatHUD->SetVisibility(ESlateVisibility::Collapsed);
		CombatHUD->SetIsEnabled(false);
	}

	// Just ensure correct state
	ValidateCombatHUDVisibility();
}

void AProdigyPlayerController::HideCombatHUD()
{
	if (!IsValid(CombatHUD)) return;

	CombatHUD->SetVisibility(ESlateVisibility::Collapsed);
	CombatHUD->SetIsEnabled(false);
}

bool AProdigyPlayerController::UI_IsInCombat() const
{
	if (UCombatSubsystem* Combat = GetCombatSubsystem())
	{
		return Combat->IsInCombat();
	}
	return false;
}

void AProdigyPlayerController::HandleCombatStateChanged_FromSubsystem(bool bNowInCombat)
{
	ValidateCombatHUDVisibility();

	SetParticipantsWorldHealthBarsVisible(bNowInCombat);
	
	OnCombatHUDDirty.Broadcast();
}

void AProdigyPlayerController::HandleTurnActorChanged_FromSubsystem(AActor* CurrentTurnActor)
{
	ValidateCombatHUDVisibility();

	if (UCombatSubsystem* Combat = GetCombatSubsystem())
	{
		SetParticipantsWorldHealthBarsVisible(Combat->IsInCombat());
	}
	
	OnCombatHUDDirty.Broadcast();
}

void AProdigyPlayerController::HandleParticipantsChanged_FromSubsystem()
{
	ValidateCombatHUDVisibility();
}

void AProdigyPlayerController::ValidateCombatHUDVisibility()
{
	APawn* MyPawn = GetPawn();
	if (!IsValid(MyPawn))
	{
		HideCombatHUD();
		return;
	}

	// Helper: ensure widget exists but do NOT force visible
	auto EnsureHUD = [this]()
	{
		if (IsValid(CombatHUD) || !CombatHUDClass) return;

		CombatHUD = CreateWidget<UUserWidget>(this, CombatHUDClass);
		if (!IsValid(CombatHUD)) return;

		CombatHUD->AddToViewport(2000);

		GetWorldTimerManager().SetTimerForNextTick([this]()
		{
			if (!IsValid(CombatHUD)) return;
			PlaceSingleWidgetBottomCenter(this, CombatHUD, /*BottomPaddingPx*/ 300.f);
		});

		// Start hidden until we decide otherwise
		CombatHUD->SetVisibility(ESlateVisibility::Collapsed);
		CombatHUD->SetIsEnabled(false);
	};

	UCombatSubsystem* Combat = GetCombatSubsystem();

	bool bShouldShow = false;

	// A) In combat: show if there's at least one enemy participant.
	// (Optional: also require that *I* am one of the participants, using controller ownership, not pointer equality.)
	if (Combat && Combat->IsInCombat())
	{
		const TArray<TWeakObjectPtr<AActor>> Parts = Combat->GetParticipants();

		bool bHasMe = false;
		bool bHasEnemy = false;

		for (const TWeakObjectPtr<AActor>& W : Parts)
		{
			AActor* A = W.Get();
			if (!IsValid(A)) continue;

			APawn* OwnPawn = Cast<APawn>(A);

			// "Me" detection: either the pawn pointer matches OR it is controlled by this PC.
			if (OwnPawn && (OwnPawn == MyPawn || OwnPawn->GetController() == this))
			{
				bHasMe = true;
				continue;
			}

			// Enemy detection:
			// - Non-player-controlled pawn counts as enemy
			// - Non-pawn combatants count as enemy too (safe default for HUD)
			const bool bIsPlayerControlled = OwnPawn && OwnPawn->IsPlayerControlled();
			if (!bIsPlayerControlled)
			{
				bHasEnemy = true;
			}
		}

		// If you don't care about bHasMe, just do: bShouldShow = bHasEnemy;
		bShouldShow = bHasMe && bHasEnemy;
	}
	else
	{
		// B) Pre-combat: show if valid locked target and not dead
		AActor* T = LockedTarget.Get();
		if (IsValid(T))
		{
			if (!bOnlyLockCombatants || T->GetClass()->ImplementsInterface(UCombatantInterface::StaticClass()))
			{
				bShouldShow = !ProdigyAbilityUtils::IsDeadByAttributes(T);
			}
		}
	}

	// If we might show, make sure it exists
	if (bShouldShow)
	{
		EnsureHUD();
	}

	// Apply state
	if (bShouldShow && IsValid(CombatHUD))
	{
		CombatHUD->SetVisibility(ESlateVisibility::Visible);
		CombatHUD->SetIsEnabled(true);
	}
	else
	{
		HideCombatHUD();
	}

	OnCombatHUDDirty.Broadcast();
}

float AProdigyPlayerController::UI_GetHP() const
{
	UAttributesComponent* Attr = GetPawn() ? GetPawn()->FindComponentByClass<UAttributesComponent>() : nullptr;
	return IsValid(Attr) ? Attr->GetCurrentValue(ProdigyTags::Attr::Health) : 0.f;
}

float AProdigyPlayerController::UI_GetMaxHP() const
{
	UAttributesComponent* Attr = GetPawn() ? GetPawn()->FindComponentByClass<UAttributesComponent>() : nullptr;
	return IsValid(Attr) ? Attr->GetFinalValue(ProdigyTags::Attr::MaxHealth) : 0.f;
}

float AProdigyPlayerController::UI_GetAP() const
{
	UAttributesComponent* Attr = GetPawn() ? GetPawn()->FindComponentByClass<UAttributesComponent>() : nullptr;
	return IsValid(Attr) ? Attr->GetCurrentValue(ProdigyTags::Attr::AP) : 0.f;
}

float AProdigyPlayerController::UI_GetMaxAP() const
{
	UAttributesComponent* Attr = GetPawn() ? GetPawn()->FindComponentByClass<UAttributesComponent>() : nullptr;
	return IsValid(Attr) ? Attr->GetFinalValue(ProdigyTags::Attr::MaxAP) : 0.f;
}

void AProdigyPlayerController::UI_StartFight()
{
	APawn* P = GetPawn();
	if (!IsValid(P)) return;

	UCombatSubsystem* Combat = GetCombatSubsystem();
	if (!Combat) return;

	AActor* Target = LockedTarget.Get();
	if (!IsValid(Target)) return;

	if (Combat->IsInCombat()) return;

	TArray<AActor*> Parts;
	Parts.Add(Target);
	Parts.Add(P);

	Combat->EnterCombat(Parts, /*FirstToAct*/ P);
}

void AProdigyPlayerController::UI_EndTurn()
{
	EndTurn();
}


void AProdigyPlayerController::ShowHotbar()
{
	if (!HotbarClass) return;

	if (!IsValid(HotbarWidget))
	{
		HotbarWidget = CreateWidget<UUserWidget>(this, HotbarClass);
		if (!IsValid(HotbarWidget)) return;

		HotbarWidget->AddToViewport(1500);

		// Position next tick after layout pass
		GetWorldTimerManager().SetTimerForNextTick([this]()
		{
			if (!IsValid(HotbarWidget)) return;

			PlaceSingleWidgetBottomCenter(
				this,
				HotbarWidget,
				/*BottomPaddingPx*/ 120.f
			);
		});
	}

	HotbarWidget->SetVisibility(ESlateVisibility::Visible);
	HotbarWidget->SetIsEnabled(true);
}

void AProdigyPlayerController::HideHotbar()
{
	if (!IsValid(HotbarWidget)) return;
	HotbarWidget->SetVisibility(ESlateVisibility::Collapsed);
	HotbarWidget->SetIsEnabled(false);
}

void AProdigyPlayerController::ShowSpellBook()
{
	if (!SpellBookClass) return;

	if (!IsValid(SpellBookWidget))
	{
		SpellBookWidget = CreateWidget<UUserWidget>(this, SpellBookClass);
		if (!IsValid(SpellBookWidget)) return;

		SpellBookWidget->AddToViewport(1600);

		GetWorldTimerManager().SetTimerForNextTick([this]()
		{
			if (!IsValid(SpellBookWidget)) return;
			PlaceSingleWidgetCenter(this, SpellBookWidget, /*CenterOffsetPx*/ FVector2D::ZeroVector);
		});
	}

	SpellBookWidget->SetVisibility(ESlateVisibility::Visible);
	SpellBookWidget->SetIsEnabled(true);

	OnCombatHUDDirty.Broadcast();
}

void AProdigyPlayerController::HideSpellBook()
{
	if (!IsValid(SpellBookWidget)) return;
	SpellBookWidget->SetVisibility(ESlateVisibility::Collapsed);
	SpellBookWidget->SetIsEnabled(false);
}
