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

	if (!InventoryComponent.IsValid())
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> InventoryComponent invalid"));
		return;
	}

	if (!OwningSkeletalMesh.IsValid())
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> OwningSkeletalMesh invalid"));
		return;
	}

	FItemRow Row;
	if (!InventoryComponent->TryGetItemDef(ItemID, Row))
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> Failed to resolve row"));
		return;
	}

	UE_LOG(LogEquipmentVisual, Warning,
	TEXT("  -> EquipActorClass Valid=%d Path=%s"),
	Row.EquipActorClass.IsValid() ? 1 : 0,
	*Row.EquipActorClass.ToString());

	if (!Row.EquipActorClass.IsValid())
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> EquipActorClass not set"));
		return;
	}

	UWorld* W = GetWorld();
	if (!W)
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> World invalid"));
		return;
	}

	TSubclassOf<AInvEquipActor> EquipClass = Row.EquipActorClass.LoadSynchronous();
	if (!EquipClass)
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> Failed to load EquipActorClass"));
		return;
	}

	AInvEquipActor* Spawned = W->SpawnActor<AInvEquipActor>(EquipClass);
	if (!IsValid(Spawned))
	{
		UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> Spawn failed"));
		return;
	}

	Spawned->SetOwner(GetOwner());
	Spawned->SetEquipmentType(EquipSlotTag);
	Spawned->SetItemID(ItemID);

	UE_LOG(LogEquipmentVisual, Warning,
		TEXT("  -> Attaching to mesh %s"),
		*GetNameSafe(OwningSkeletalMesh.Get()));

	Spawned->AttachToComponent(
		OwningSkeletalMesh.Get(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale
	);

	EquippedActors.Add(Spawned);

	UE_LOG(LogEquipmentVisual, Warning, TEXT("  -> Equip visual attached"));
}

void UInvEquipmentComponent::OnItemUnequipped(FGameplayTag EquipSlotTag, FName ItemID)
{
	if (!EquipSlotTag.IsValid()) return;
	RemoveEquippedActor(EquipSlotTag);
}
