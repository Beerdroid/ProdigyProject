// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Inv_PlayerController.generated.h"

class UInv_InventoryComponent;
class UInputMappingContext;
class UInputAction;
class UInv_HUDWidget;
class UInv_ItemComponent;

UCLASS()
class INVENTORY_API AInv_PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AInv_PlayerController();
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Interaction")
	AActor* GetHoveredActor() const { return ThisActor.Get(); }
	
	UFUNCTION(BlueprintCallable, Server, Reliable)
	void Server_StartTrade(AActor* MerchantActor);

	UFUNCTION(BlueprintCallable, Client, Reliable)
	void Client_OpenMerchantTrade(AActor* Merchant);

	UFUNCTION(Server, Reliable)
	void Server_BuyFromMerchant(AActor* MerchantActor, FName ItemID, int32 Quantity);

	UFUNCTION(Server, Reliable)
	void Server_SellToMerchant(AActor* MerchantActor, FName ItemID, int32 Quantity);


protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// ---- Movement (TopDown port) ----
	UPROPERTY(EditDefaultsOnly, Category="Input|Movement")
	TObjectPtr<UInputAction> SetDestinationClickAction;

	UPROPERTY(EditDefaultsOnly, Category="Input|Movement")
	float PressedThreshold = 0.20f; // short click vs hold threshold

	// Optional: click FX like template
	UPROPERTY(EditDefaultsOnly, Category="Movement|FX")
	TObjectPtr<UNiagaraSystem> FXCursor = nullptr;

	TWeakObjectPtr<UInv_InventoryComponent> InventoryComponent;

	bool IsWithinPickupDistance(const AActor* TargetActor) const;

	void StartMoveToPickup(AActor* TargetActor, UInv_ItemComponent* ItemComp);

	bool TryPrimaryPickup(AActor* TargetActor);
private:
	// ---- Inventory / UI ----
	virtual void PrimaryInteract();

	void CreateHUDWidget();

	// ---- Tracing (hover message/highlight) ----
	void TraceForItem();
	void TraceUnderMouseForItem();
	FTimerHandle TraceTimerHandle;

	// ---- Inventory component ----

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TObjectPtr<UInputMappingContext> DefaultIMC;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TObjectPtr<UInputAction> PrimaryInteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UInv_HUDWidget> HUDWidgetClass;

	UPROPERTY()
	TObjectPtr<UInv_HUDWidget> HUDWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	double TraceLength;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TEnumAsByte<ECollisionChannel> ItemTraceChannel;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TEnumAsByte<ECollisionChannel> InteractTraceChannel;

	TWeakObjectPtr<AActor> ThisActor;
	TWeakObjectPtr<AActor> LastActor;

	// ---- Pickup distance / move-to-pickup ----
	UPROPERTY(EditDefaultsOnly, Category="Inventory")
	float MaxPickupDistance = 200.0f;

	TWeakObjectPtr<AActor> PendingPickupActor;
	TWeakObjectPtr<UInv_ItemComponent> PendingPickupItemComp;
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


};
