#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InventoryComponent.h"
#include "InvPlayerController.generated.h"

class UNiagaraSystem;
class UInputAction;
class UInputMappingContext;
class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveExternalInventoryChanged, UInventoryComponent*, NewExternal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoldChanged, int32, NewGold);

UENUM(BlueprintType)
enum class EInvAction : uint8
{
	DragDrop,     // normal drop
	QuickUse,     // right click
	AutoMove      // shift click
};

UCLASS()
class INVENTORY_API AInvPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// ===== Config / refs (InventoryComponent is added in BP) =====

	/** Set this in BP (or let it auto-find in BeginPlay). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	TWeakObjectPtr<UInventoryComponent> InventoryComponent = nullptr;

	/** Currently opened external inventory: container/merchant/loot bag. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	TWeakObjectPtr<UInventoryComponent> ExternalInventory = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TObjectPtr<UInputMappingContext> DefaultIMC;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TObjectPtr<UInputAction> PrimaryInteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	UPROPERTY(EditDefaultsOnly, Category="Input|Movement")
	TObjectPtr<UInputAction> SetDestinationClickAction;

	/** Optional: where to spawn dropped items */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory")
	float DropDistance = 150.f;

	// ===== Events for UI =====
	UPROPERTY(BlueprintAssignable, Category="Inventory|Events")
	FOnActiveExternalInventoryChanged OnExternalInventoryChanged;

	// ===== Lifecycle =====
	virtual void BeginPlay() override;

	void SetupInputComponent();

	// ---- Tracing (hover message/highlight) ----
	
	void TraceForItem();
	void TraceUnderMouseForItem();
	FTimerHandle TraceTimerHandle;

	/** Set external inventory explicitly (e.g., when interacting with a container). */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void SetExternalInventory(UInventoryComponent* NewExternal);

	/** Clears the external inventory reference (e.g., when closing container UI). */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void ClearExternalInventory();

	/** Convenience: try to pull inventory from an actor implementing InventoryOwnerInterface */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool SetExternalInventoryFromActor(AActor* ActorWithInventory);

	// ===== Actions: Player inventory only =====
	UFUNCTION(BlueprintCallable, Category="Inventory|Actions")
	bool Player_MoveOrSwap(int32 FromIndex, int32 ToIndex, TArray<int32>& OutChanged);

	UFUNCTION(BlueprintCallable, Category="Inventory|Actions")
	bool Player_SplitStack(int32 FromIndex, int32 ToIndex, int32 SplitAmount, TArray<int32>& OutChanged);

	UFUNCTION(BlueprintCallable, Category="Inventory|Actions")
	bool Player_DropFromSlot(int32 SlotIndex, int32 Quantity, TArray<int32>& OutChanged);

	// ===== Actions: Player <-> External =====
	UFUNCTION(BlueprintCallable, Category="Inventory|Actions")
	bool Transfer_PlayerToExternal(int32 PlayerFromIndex, int32 ExternalToIndex, int32 Quantity,
	                               TArray<int32>& OutChangedPlayer, TArray<int32>& OutChangedExternal);

	UFUNCTION(BlueprintCallable, Category="Inventory|Actions")
	bool Transfer_ExternalToPlayer(int32 ExternalFromIndex, int32 PlayerToIndex, int32 Quantity,
	                               TArray<int32>& OutChangedExternal, TArray<int32>& OutChangedPlayer);

	UFUNCTION(BlueprintCallable, Category="Inventory|Actions")
	bool AutoTransfer_PlayerToExternal(int32 PlayerFromIndex, int32 Quantity,
	                                   TArray<int32>& OutChangedPlayer, TArray<int32>& OutChangedExternal);

	UFUNCTION(BlueprintCallable, Category="Inventory|Actions")
	bool AutoTransfer_ExternalToPlayer(int32 ExternalFromIndex, int32 Quantity,
	                                   TArray<int32>& OutChangedExternal, TArray<int32>& OutChangedPlayer);

	// ===== Optional: Use/Consume from player slot =====
	UFUNCTION(BlueprintCallable, Category="Inventory|Actions")
	virtual bool ConsumeFromSlot(int32 SlotIndex, TArray<int32>& OutChanged);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	double TraceLength;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TEnumAsByte<ECollisionChannel> ItemTraceChannel;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TEnumAsByte<ECollisionChannel> InteractTraceChannel;

	bool IsWithinPickupDistance(const AActor* TargetActor) const;
	virtual void PrimaryInteract();
	bool TryPrimaryPickup(AActor* TargetActor);

	void StartMoveToPickup(AActor* TargetActor, UItemComponent* ItemComp);

	TWeakObjectPtr<AActor> ThisActor;
	TWeakObjectPtr<AActor> LastActor;

	UPROPERTY(EditDefaultsOnly, Category="Inventory")
	float MaxPickupDistance = 200.0f;

	TWeakObjectPtr<AActor> PendingPickupActor;
	TWeakObjectPtr<UItemComponent> PendingPickupItemComp;
	FTimerHandle PendingPickupTimerHandle;

	void TickPendingPickup();
	void ClearPendingPickup();

	// ---- Movement state ----
	FVector CachedDestination = FVector::ZeroVector;
	double PressStartTimeSeconds = 0.0;
	bool bBlockMoveThisClick = false;

	// ---- Input handlers ----
	void OnSetDestinationStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationCompleted();
	void OnSetDestinationCanceled();

	// ---- Helpers ----
	bool GetLocationUnderCursor(FVector& OutLocation) const;
	void Follow(const FVector& Location);
	void MoveTo(const FVector& Location);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Interaction")
	AActor* GetHoveredActor() const { return ThisActor.Get(); }

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UUserWidget> PlayerInventoryWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UUserWidget> ExternalInventoryWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> PlayerInventoryWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> ExternalInventoryWidget;

	UFUNCTION(BlueprintCallable)
	void SetInventory(UInventoryComponent* InInventory);

	UFUNCTION(BlueprintCallable)
	void OpenInventory();

	UFUNCTION(BlueprintCallable)
	void CloseInventory();

	UFUNCTION(BlueprintCallable)
	void ToggleInventory();

	UPROPERTY(EditDefaultsOnly, Category="UI")
	int32 MaxInteractDistance = 200;

	bool CanUseExternalInventory(const UInventoryComponent* External, const AActor* ExternalOwner) const;

	UFUNCTION(BlueprintCallable)
	void OpenExternalInventoryFromActor(AActor* ActorWithInventory);

	UFUNCTION(BlueprintCallable)
	void CloseExternalInventory();

	UPROPERTY()
	bool bInventoryOpen;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool ExecuteInventoryAction(
		UInventoryComponent* Source,
		int32 SourceIndex,
		UInventoryComponent* Target,
		int32 TargetIndex,
		int32 Quantity,
		EInvAction Action,
		bool bAllowSwap = true,
		bool bFullOnly = false);

	void EnsurePlayerInventoryResolved();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Economy")
	int32 Gold = 0;

	UFUNCTION(BlueprintCallable, Category="Economy")
	bool CanAfford(int32 Amount) const { return Amount >= 0 && Gold >= Amount; }

	UFUNCTION(BlueprintCallable, Category="Economy")
	void AddGold(int32 Delta);

	UFUNCTION(BlueprintCallable, Category="Economy")
	void SetGold(int32 NewGold);

	UFUNCTION(BlueprintCallable, Category="Economy")
	bool TryGetItemPrice(FName ItemID, int32& OutSellValue) const;

	UPROPERTY(BlueprintAssignable, Category="Economy|Events")
	FOnGoldChanged OnGoldChanged;

	virtual bool HandlePrimaryClickActor(AActor* ClickedActor) { return false; }

	virtual AActor* GetActorUnderCursorForClick() const;

private:
	UPROPERTY(EditDefaultsOnly, Category="Movement|FX")
	TObjectPtr<UNiagaraSystem> FXCursor = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Input|Movement")
	float PressedThreshold = 0.20f; // short click vs hold threshold

	UPROPERTY(EditDefaultsOnly, Category="Inventory|Drop")
	float DropMinRadius = 80.f;

	UPROPERTY(EditDefaultsOnly, Category="Inventory|Drop")
	float DropMaxRadius = 150.f;

	UPROPERTY(EditDefaultsOnly, Category="Inventory|Drop")
	float DropHeightTrace = 5000.f;
	


	/** A simple drop point in front of the camera/controller. */
	void GetDropTransform(FVector& OutLoc, FRotator& OutRot) const;
};
