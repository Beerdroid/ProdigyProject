#include "ProdigyInventory/InvEquipmentComponent.h"

#include "ProdigyInventory/InvEquipActor.h"
#include "ProdigyInventory/InventoryComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/ProdigyInventory/EquipSlotWidget.h"

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

	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("[EquipComp BeginPlay] Owner=%s"),
		*GetNameSafe(GetOwner()));

	// Resolve player inventory (it lives on PC)
	InitInventoryComponent();

	// Resolve skeletal mesh from owner (your log shows owner is BP_TopDownCharacter)
	if (!OwningSkeletalMesh.IsValid())
	{
		if (ACharacter* C = Cast<ACharacter>(GetOwner()))
		{
			USkeletalMeshComponent* M = C->GetMesh();
			OwningSkeletalMesh = M;

			UE_LOG(LogEquipmentVisual, Warning,
				TEXT("[EquipComp BeginPlay] Resolved mesh from Character: %s"),
				*GetNameSafe(M));
		}
		else
		{
			UE_LOG(LogEquipmentVisual, Warning,
				TEXT("[EquipComp BeginPlay] Owner is not ACharacter, cannot auto-resolve mesh"));
		}
	}
}

UInventoryComponent* UInvEquipmentComponent::ResolvePlayerInventory() const
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;

	APlayerController* PC = UGameplayStatics::GetPlayerController(W, 0);
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

	if (!InventoryComponent->OnItemEquipped.IsAlreadyBound(this, &ThisClass::OnItemEquipped))
	{
		InventoryComponent->OnItemEquipped.AddDynamic(this, &ThisClass::OnItemEquipped);
		UE_LOG(LogEquipmentVisual, Warning, TEXT("[InitInventoryComponent] Bound OnItemEquipped"));
	}

	if (!InventoryComponent->OnItemUnequipped.IsAlreadyBound(this, &ThisClass::OnItemUnequipped))
	{
		InventoryComponent->OnItemUnequipped.AddDynamic(this, &ThisClass::OnItemUnequipped);
		UE_LOG(LogEquipmentVisual, Warning, TEXT("[InitInventoryComponent] Bound OnItemUnequipped"));
	}
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

void UInvEquipmentComponent::OnItemEquipped(FGameplayTag EquipSlotTag, FName ItemID)
{
	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("[OnItemEquipped] SlotTag=%s ItemID=%s Owner=%s"),
		*EquipSlotTag.ToString(),
		*ItemID.ToString(),
		*GetNameSafe(GetOwner()));

	// ---- basic gates
	if (!EquipSlotTag.IsValid() || ItemID.IsNone())
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> BAD INPUT SlotTagValid=%d ItemIDNone=%d"),
			EquipSlotTag.IsValid() ? 1 : 0,
			ItemID.IsNone() ? 1 : 0);
		return;
	}

	if (!InventoryComponent.IsValid())
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> InventoryComponent INVALID"));
		return;
	}

	if (!OwningSkeletalMesh.IsValid())
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> OwningSkeletalMesh INVALID"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> World INVALID"));
		return;
	}

	// ---- resolve item row
	FItemRow Row;
	if (!InventoryComponent->TryGetItemDef(ItemID, Row))
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> TryGetItemDef FAILED for ItemID=%s"), *ItemID.ToString());
		return;
	}

	// log what we got (helps detect “wrong table/row” instantly)
	const FSoftObjectPath SoftPath = Row.EquipActorClass.ToSoftObjectPath();
	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("  -> Row: EquipSlotTag=%s  EquipAttachSocket=%s"),
		*Row.EquipSlotTag.ToString(),
		*Row.EquipAttachSocket.ToString());

	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("  -> EquipActorClass: IsNull=%d IsValidPath=%d Path=%s"),
		Row.EquipActorClass.IsNull() ? 1 : 0,
		SoftPath.IsValid() ? 1 : 0,
		*SoftPath.ToString());

	if (Row.EquipActorClass.IsNull())
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> EquipActorClass is NULL in DT row"));
		return;
	}

	// ---- replace old visual first
	RemoveEquippedActor(EquipSlotTag);

	// ---- load class (primary path)
	TSubclassOf<AInvEquipActor> EquipClass = Row.EquipActorClass.LoadSynchronous();

	// fallback load (helps when redirectors / stale paths / weird DT serialization)
	if (!EquipClass)
	{
		const FString PathStr = SoftPath.ToString();
		UE_LOG(LogEquipmentVisual, Warning,
			TEXT("  -> LoadSynchronous FAILED, trying StaticLoadClass on path: %s"),
			*PathStr);

		if (!PathStr.IsEmpty())
		{
			UClass* Loaded = StaticLoadClass(AInvEquipActor::StaticClass(), nullptr, *PathStr);
			EquipClass = Loaded;
		}
	}

	if (!EquipClass)
	{
		UE_LOG(LogEquipmentVisual, Error,
			TEXT("  -> FAILED to load EquipActorClass for ItemID=%s. SoftPath=%s"),
			*ItemID.ToString(),
			*SoftPath.ToString());
		return;
	}

	USkeletalMeshComponent* Mesh = OwningSkeletalMesh.Get();
	if (!Mesh)
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> Mesh NULL after IsValid??"));
		return;
	}

	// ---- spawn at mesh transform to avoid “on floor for 1 frame”
	const FTransform SpawnTM = Mesh->GetComponentTransform();

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AInvEquipActor* Spawned = World->SpawnActor<AInvEquipActor>(EquipClass, SpawnTM, Params);
	if (!IsValid(Spawned))
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> SpawnActor FAILED"));
		return;
	}

	Spawned->SetOwner(GetOwner());
	Spawned->SetEquipmentType(EquipSlotTag);
	Spawned->SetItemID(ItemID);

	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("  -> Attaching to mesh=%s  Socket=%s"),
		*GetNameSafe(Mesh),
		*Row.EquipAttachSocket.ToString());

	// ---- attach
	const FName SocketName = Row.EquipAttachSocket;

	if (SocketName != NAME_None)
	{
		const bool bSocketExists = Mesh->DoesSocketExist(SocketName);
		UE_LOG(LogEquipmentVisual, Warning,
			TEXT("  -> SocketExists=%d"),
			bSocketExists ? 1 : 0);

		if (bSocketExists)
		{
			Spawned->AttachToComponent(
				Mesh,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				SocketName
			);
		}
		else
		{
			// socket requested but missing -> fallback to root attach
			Spawned->AttachToComponent(
				Mesh,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale
			);
		}
	}
	else
	{
		// no socket -> root attach (your “cloak” case)
		Spawned->AttachToComponent(
			Mesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale
		);
	}

	// ---- enforce clean relative after attach (prevents “kept spawn offset” cases)
	Spawned->SetActorRelativeLocation(FVector::ZeroVector);
	Spawned->SetActorRelativeRotation(FRotator::ZeroRotator);
	Spawned->SetActorRelativeScale3D(FVector::OneVector);

	EquippedActors.Add(Spawned);

	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("  -> OK Attached. ParentSocket=%s WorldLoc=%s"),
		*Spawned->GetAttachParentSocketName().ToString(),
		*Spawned->GetActorLocation().ToString());
}


void UInvEquipmentComponent::OnItemUnequipped(FGameplayTag EquipSlotTag, FName ItemID)
{
	if (!EquipSlotTag.IsValid()) return;
	RemoveEquippedActor(EquipSlotTag);
}
