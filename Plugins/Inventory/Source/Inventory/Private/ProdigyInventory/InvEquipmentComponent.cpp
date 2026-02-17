#include "ProdigyInventory/InvEquipmentComponent.h"

#include "ProdigyInventory/InvEquipActor.h"
#include "ProdigyInventory/InventoryComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipmentVisual, Log, All);

UInvEquipmentComponent::UInvEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInvEquipmentComponent::SetOwningSkeletalMesh(USkeletalMeshComponent* OwningMesh)
{
	OwningSkeletalMesh = OwningMesh;
}

void UInvEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningPC = Cast<APlayerController>(GetOwner());

	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("[EquipComp BeginPlay] Owner=%s PC=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(OwningPC.Get()));

	// 1) Inventory lives on this PC now
	InitInventoryComponent();

	// 2) Mesh comes from pawn (not owner anymore)
	if (OwningPC.IsValid())
	{
		// If pawn already exists, resolve immediately
		ResolveMeshFromPawn(OwningPC->GetPawn());

		// Also bind to pawn changes
		OwningPC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
		OwningPC->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	// 3) If you want visuals to appear when opening PIE / after moving comp, replay:
	RebuildAllEquippedVisuals();
}

void UInvEquipmentComponent::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("[EquipComp] PossessedPawnChanged Old=%s New=%s"),
		*GetNameSafe(OldPawn),
		*GetNameSafe(NewPawn));

	ResolveMeshFromPawn(NewPawn);

	// Re-attach everything to new mesh
	RebuildAllEquippedVisuals();
}

void UInvEquipmentComponent::ResolveMeshFromPawn(APawn* Pawn)
{
	OwningSkeletalMesh = nullptr;

	ACharacter* C = Cast<ACharacter>(Pawn);
	if (!IsValid(C))
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("[EquipComp] ResolveMeshFromPawn: Pawn is not Character"));
		return;
	}

	USkeletalMeshComponent* M = C->GetMesh();
	OwningSkeletalMesh = M;

	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("[EquipComp] Resolved mesh from Pawn: %s"),
		*GetNameSafe(M));
}

UInventoryComponent* UInvEquipmentComponent::ResolvePlayerInventory() const
{
	// PC-owned now: no GetPlayerController(0)
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!IsValid(PC)) return nullptr;

	return PC->FindComponentByClass<UInventoryComponent>();
}

void UInvEquipmentComponent::InitInventoryComponent()
{
	UInventoryComponent* Inv = ResolvePlayerInventory();
	InventoryComponent = Inv;

	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("[InitInventoryComponent] Resolved Inventory=%s"),
		*GetNameSafe(Inv));

	if (!InventoryComponent.IsValid()) return;

	InventoryComponent->OnItemEquipped.RemoveAll(this);
	InventoryComponent->OnItemUnequipped.RemoveAll(this);

	InventoryComponent->OnItemEquipped.AddDynamic(this, &ThisClass::OnItemEquipped);
	InventoryComponent->OnItemUnequipped.AddDynamic(this, &ThisClass::OnItemUnequipped);

	UE_LOG(LogEquipmentVisual, Warning, TEXT("[InitInventoryComponent] Bound equip delegates"));
}

AInvEquipActor* UInvEquipmentComponent::FindEquippedActor(const FGameplayTag& EquipSlotTag) const
{
	if (!EquipSlotTag.IsValid()) return nullptr;

	for (AInvEquipActor* A : EquippedActors)
	{
		if (IsValid(A) && A->GetEquipmentType().MatchesTagExact(EquipSlotTag))
		{
			return A;
		}
	}
	return nullptr;
}

void UInvEquipmentComponent::RemoveEquippedActor(const FGameplayTag& EquipSlotTag)
{
	if (AInvEquipActor* Existing = FindEquippedActor(EquipSlotTag); IsValid(Existing))
	{
		EquippedActors.Remove(Existing);
		Existing->Destroy();
	}
}

void UInvEquipmentComponent::RebuildAllEquippedVisuals()
{
	// Safe “replay” after pawn changes / begin play.
	// Requires you to know which slot tags exist.
	// If your InventoryComponent has EquippedItems array, iterate it instead (recommended).

	if (!InventoryComponent.IsValid())
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("[RebuildAllEquippedVisuals] InventoryComponent invalid"));
		return;
	}

	if (!OwningSkeletalMesh.IsValid())
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("[RebuildAllEquippedVisuals] Mesh invalid"));
		return;
	}

	// Clear any spawned visuals (optional; avoids duplicates)
	for (AInvEquipActor* A : EquippedActors)
	{
		if (IsValid(A)) A->Destroy();
	}
	EquippedActors.Reset();

	// === OPTION 1 (best): iterate EquippedItems array if you have it
	// Example if you add:
	// const TArray<FEquippedItemEntry>& UInventoryComponent::GetEquippedItems() const;
	//
	// for (const FEquippedItemEntry& E : InventoryComponent->GetEquippedItems())
	// {
	//     if (E.EquipSlotTag.IsValid() && !E.ItemID.IsNone())
	//     {
	//         OnItemEquipped(E.EquipSlotTag, E.ItemID);
	//     }
	// }

	// === OPTION 2: if you only have GetEquippedItem(tag,...), you must have a known slot list somewhere.
	// (Skipping here to avoid inventing your tag set.)
}

void UInvEquipmentComponent::OnItemEquipped(FGameplayTag EquipSlotTag, FName ItemID)
{
	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("[OnItemEquipped] SlotTag=%s ItemID=%s Owner(PC)=%s Pawn=%s"),
		*EquipSlotTag.ToString(),
		*ItemID.ToString(),
		*GetNameSafe(GetOwner()),
		OwningPC.IsValid() ? *GetNameSafe(OwningPC->GetPawn()) : TEXT("None"));

	if (!EquipSlotTag.IsValid() || ItemID.IsNone()) return;
	if (!InventoryComponent.IsValid()) return;
	if (!OwningSkeletalMesh.IsValid())
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> Mesh invalid (pawn not resolved yet?)"));
		return;
	}

	FItemRow Row;
	if (!InventoryComponent->TryGetItemDef(ItemID, Row))
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> TryGetItemDef FAILED for %s"), *ItemID.ToString());
		return;
	}

	if (Row.EquipActorClass.IsNull())
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> EquipActorClass NULL in row for %s"), *ItemID.ToString());
		return;
	}

	RemoveEquippedActor(EquipSlotTag);

	TSubclassOf<AInvEquipActor> EquipClass = Row.EquipActorClass.LoadSynchronous();
	if (!EquipClass)
	{
		UE_LOG(LogEquipmentVisual, Error, TEXT("  -> LoadSynchronous FAILED for %s"), *Row.EquipActorClass.ToString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	USkeletalMeshComponent* Mesh = OwningSkeletalMesh.Get();

	// Spawn near mesh to avoid “floor for 1 frame”
	const FTransform SpawnTM = Mesh->GetComponentTransform();

	FActorSpawnParameters Params;
	Params.Owner = OwningPC.IsValid() ? Cast<AActor>(OwningPC.Get()) : GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AInvEquipActor* Spawned = World->SpawnActor<AInvEquipActor>(EquipClass, SpawnTM, Params);
	if (!IsValid(Spawned)) return;

	Spawned->SetOwner(OwningPC.IsValid() ? Cast<AActor>(OwningPC.Get()) : GetOwner());
	Spawned->SetEquipmentType(EquipSlotTag);
	Spawned->SetItemID(ItemID);

	const FName SocketName = Row.EquipAttachSocket;

	if (SocketName != NAME_None && Mesh->DoesSocketExist(SocketName))
	{
		Spawned->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
	}
	else
	{
		// root attach (cloak case)
		Spawned->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	Spawned->SetActorRelativeLocation(FVector::ZeroVector);
	Spawned->SetActorRelativeRotation(FRotator::ZeroRotator);
	Spawned->SetActorRelativeScale3D(FVector::OneVector);

	EquippedActors.Add(Spawned);

	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("  -> OK Attached Socket=%s WorldLoc=%s"),
		*Spawned->GetAttachParentSocketName().ToString(),
		*Spawned->GetActorLocation().ToString());
}

void UInvEquipmentComponent::OnItemUnequipped(FGameplayTag EquipSlotTag, FName ItemID)
{
	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("[OnItemUnequipped] SlotTag=%s ItemID=%s"),
		*EquipSlotTag.ToString(),
		*ItemID.ToString());

	if (!EquipSlotTag.IsValid()) return;
	RemoveEquippedActor(EquipSlotTag);
}

TArray<AInvEquipActor*> UInvEquipmentComponent::GetEquippedActorsCopy() const
{
	TArray<AInvEquipActor*> Out;
	Out.Reserve(EquippedActors.Num());

	for (AInvEquipActor* A : EquippedActors)
	{
		if (IsValid(A))
		{
			Out.Add(A);
		}
	}
	return Out;
}
