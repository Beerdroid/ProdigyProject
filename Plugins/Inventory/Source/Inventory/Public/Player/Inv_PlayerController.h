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

private:
	// ---- Inventory / UI ----
	void PrimaryInteract();
	void CreateHUDWidget();

	// ---- Tracing (hover message/highlight) ----
	void TraceForItem();
	void TraceUnderMouseForItem();
	FTimerHandle TraceTimerHandle;

	// ---- Inventory component ----
	TWeakObjectPtr<UInv_InventoryComponent> InventoryComponent;

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

	TWeakObjectPtr<AActor> ThisActor;
	TWeakObjectPtr<AActor> LastActor;

	// ---- Pickup distance / move-to-pickup ----
	UPROPERTY(EditDefaultsOnly, Category="Inventory")
	float MaxPickupDistance = 200.0f;

	TWeakObjectPtr<AActor> PendingPickupActor;
	TWeakObjectPtr<UInv_ItemComponent> PendingPickupItemComp;
	FTimerHandle PendingPickupTimerHandle;

	bool TryPickupItemUnderMouse_Refresh();
	void StartMoveToPickup(AActor* TargetActor, UInv_ItemComponent* ItemComp);
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

	bool IsWithinPickupDistance(const AActor* TargetActor) const;
};
