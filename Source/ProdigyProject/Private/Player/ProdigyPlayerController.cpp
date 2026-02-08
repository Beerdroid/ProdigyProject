#include "Player/ProdigyPlayerController.h"

#include "Quest/QuestLogComponent.h"
#include "Quest/Integration/QuestIntegrationComponent.h"
#include "GameFramework/Actor.h"
#include "Interfaces/UInv_Interactable.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"

AProdigyPlayerController::AProdigyPlayerController()
{
	// Nothing required here; components can be added in BP.
}

void AProdigyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	CacheQuestComponents();

	UE_LOG(LogTemp, Warning, TEXT("ProdigyPC BeginPlay: %s Local=%d Pawn=%s"),
	*GetNameSafe(this), IsLocalController(), *GetNameSafe(GetPawn()));
}

void AProdigyPlayerController::CacheQuestComponents()
{
	// If you add them as components in BP child, FindComponentByClass will find them.
	if (!QuestLog)
	{
		QuestLog = FindComponentByClass<UQuestLogComponent>();
	}

	if (!QuestIntegration)
	{
		QuestIntegration = FindComponentByClass<UQuestIntegrationComponent>();
	}

	// Optional: log once if missing
	if (!QuestLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProdigyPlayerController: QuestLogComponent not found on %s"), *GetNameSafe(this));
	}
}

void AProdigyPlayerController::NotifyQuestsInventoryChanged()
{
	// If we are already on server, call directly; otherwise route to server.
	if (HasAuthority())
	{
		Server_NotifyQuestsInventoryChanged(); // safe even on server
	}
	else
	{
		Server_NotifyQuestsInventoryChanged();
	}
}

void AProdigyPlayerController::NotifyQuestsKillTag(FGameplayTag TargetTag)
{
	if (!TargetTag.IsValid())
	{
		return;
	}

	if (HasAuthority())
	{
		Server_NotifyQuestsKillTag(TargetTag);
	}
	else
	{
		Server_NotifyQuestsKillTag(TargetTag);
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
	AActor* Target = GetHoveredActor(); // getter from base, or use ThisActor.Get() if protected
	if (!IsValid(Target))
	{
		return;
	}

	// 1) Try pickup first (keeps old inventory behavior)
	if (TryPrimaryPickup(Target))
	{
		return;
	}

	// 2) Otherwise do generic interaction
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

void AProdigyPlayerController::Server_NotifyQuestsInventoryChanged_Implementation()
{
	CacheQuestComponents();

	if (!QuestLog)
	{
		return;
	}

	// This should be a server-only function on the QuestLog that recomputes collect objectives and advances stages.
	QuestLog->NotifyInventoryChanged();
}

void AProdigyPlayerController::Server_NotifyQuestsKillTag_Implementation(FGameplayTag TargetTag)
{
	if (!TargetTag.IsValid())
	{
		return;
	}

	CacheQuestComponents();

	if (!QuestLog)
	{
		return;
	}

	// You can expose a BlueprintCallable server function instead, but this is fine if your QuestLog uses this signature.
	QuestLog->NotifyKillObjectiveTag(TargetTag);
}
