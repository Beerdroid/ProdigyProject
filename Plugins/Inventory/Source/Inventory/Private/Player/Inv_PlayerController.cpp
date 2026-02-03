// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/Inv_PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Interaction/Inv_Highlightable.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/HUD/Inv_HUDWidget.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraFunctionLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogInvTrace, Log, All);

AInv_PlayerController::AInv_PlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	TraceLength = 500.0;
	ItemTraceChannel = ECC_GameTraceChannel1;
}

void AInv_PlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Optional alternative trace method:
	// TraceForItem();
}

void AInv_PlayerController::Server_BuyFromMerchant_Implementation(AActor* MerchantActor, FName ItemID, int32 Quantity)
{
	if (!IsValid(MerchantActor) || ItemID.IsNone() || Quantity <= 0) return;
	if (!InventoryComponent.IsValid()) return;

	UInv_InventoryComponent* MerchantInv = MerchantActor->FindComponentByClass<UInv_InventoryComponent>();
	if (!IsValid(MerchantInv)) return;

	// 1) Verify merchant has stock
	const int32 MerchantQty = MerchantInv->GetTotalQuantityByItemID(ItemID);
	if (MerchantQty < Quantity) return;

	// 2) Compute price (you need a price source — typically a fragment like SellValue/BuyValue)
	// For now: TODO
	const int32 TotalPrice = /*ResolveBuyPrice(ItemID)*/ 1 * Quantity;

	// 3) Check player money
	// if (!TrySpendMoney(TotalPrice)) return;

	// 4) Add item to player inventory (server-side)
	int32 Remainder = 0;
	const bool bAdded = InventoryComponent->AddItemByID_ServerAuth(ItemID, Quantity, MerchantActor, Remainder);

	const int32 ActuallyAdded = Quantity - Remainder;
	if (!bAdded || ActuallyAdded <= 0)
	{
		// Refund full amount if nothing added; if partial is allowed, refund the remainder portion.
		// AddMoney(TotalPrice);
		return;
	}

	// 5) Remove items from merchant for amount actually added
	MerchantInv->Server_RemoveItemByID(ItemID, ActuallyAdded, this);

	// 6) Refund for remainder if partial add happened
	if (Remainder > 0)
	{
		const int32 Refund = /*UnitPrice*/ 1 * Remainder;
		// AddMoney(Refund);
	}
}

void AInv_PlayerController::Server_SellToMerchant_Implementation(AActor* MerchantActor, FName ItemID, int32 Quantity)
{
	UE_LOG(LogTemp, Warning, TEXT("[INV][Server_Sell] PC=%s Merchant=%s ItemID=%s Qty=%d Auth=%d"),
		*GetNameSafe(this), *GetNameSafe(MerchantActor), *ItemID.ToString(), Quantity, HasAuthority());

	if (!IsValid(MerchantActor) || ItemID.IsNone() || Quantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[INV][Server_Sell] fail: invalid args"));
		return;
	}

	if (!InventoryComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[INV][Server_Sell] fail: InventoryComponent invalid"));
		return;
	}

	UInv_InventoryComponent* MerchantInv = MerchantActor->FindComponentByClass<UInv_InventoryComponent>();
	UE_LOG(LogTemp, Warning, TEXT("[INV][Server_Sell] MerchantInv=%s"), *GetNameSafe(MerchantInv));

	if (!IsValid(MerchantInv))
	{
		UE_LOG(LogTemp, Warning, TEXT("[INV][Server_Sell] fail: MerchantInv missing"));
		return;
	}

	const int32 PlayerQty = InventoryComponent->GetTotalQuantityByItemID(ItemID);
	UE_LOG(LogTemp, Warning, TEXT("[INV][Server_Sell] PlayerQty=%d"), PlayerQty);

	if (PlayerQty < Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("[INV][Server_Sell] fail: not enough quantity"));
		return;
	}

	// 2) Price (sell price)
	const int32 TotalGain = /*ResolveSellPrice(ItemID)*/ 1 * Quantity;

	// 3) Remove from player
	InventoryComponent->Server_RemoveItemByID(ItemID, Quantity, MerchantActor);

	// 4) Add to merchant
	int32 Remainder = 0;
	MerchantInv->AddItemByID_ServerAuth(ItemID, Quantity, this, Remainder);

	// If merchant has capacity limits later, handle remainder:
	// - if remainder > 0: give back items to player OR drop them OR just don’t allow sell.
	if (Remainder > 0)
	{
		// safest: give back
		int32 GiveBackRemainder = 0;
		InventoryComponent->AddItemByID_ServerAuth(ItemID, Remainder, this, GiveBackRemainder);
		// If GiveBackRemainder > 0 here, you’ve got bigger capacity issues — decide policy.
		return;
	}

	// 5) Pay player
	// AddMoney(TotalGain);
}

void AInv_PlayerController::BeginPlay()
{
	Super::BeginPlay();

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem))
	{
		Subsystem->AddMappingContext(DefaultIMC, 0);
	}

	InventoryComponent = FindComponentByClass<UInv_InventoryComponent>();
	CreateHUDWidget();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	// Hover highlight + pickup message
	GetWorldTimerManager().SetTimer(
		TraceTimerHandle,
		this,
		&AInv_PlayerController::TraceUnderMouseForItem,
		0.05f,
		true
	);
}

void AInv_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UE_LOG(LogTemp, Warning, TEXT("SetupInputComponent called on %s (Class=%s)"),
		*GetNameSafe(this), *GetClass()->GetName());

	UEnhancedInputComponent* EnhancedInputComponent =
		CastChecked<UEnhancedInputComponent>(InputComponent);

	UE_LOG(LogTemp, Warning, TEXT("Binding PrimaryInteractAction=%s"),
		*GetNameSafe(PrimaryInteractAction));

	EnhancedInputComponent->BindAction(
		PrimaryInteractAction, ETriggerEvent::Started, this, &AInv_PlayerController::PrimaryInteract);
	EnhancedInputComponent->BindAction(
		ToggleInventoryAction, ETriggerEvent::Started, this, &AInv_PlayerController::ToggleInventory);

	// TopDown click/hold movement
	if (SetDestinationClickAction)
	{
		EnhancedInputComponent->BindAction(
			SetDestinationClickAction, ETriggerEvent::Started, this, &AInv_PlayerController::OnSetDestinationStarted);
		EnhancedInputComponent->BindAction(
			SetDestinationClickAction, ETriggerEvent::Triggered, this, &AInv_PlayerController::OnSetDestinationTriggered);
		EnhancedInputComponent->BindAction(
			SetDestinationClickAction, ETriggerEvent::Completed, this, &AInv_PlayerController::OnSetDestinationCompleted);
		EnhancedInputComponent->BindAction(
			SetDestinationClickAction, ETriggerEvent::Canceled, this, &AInv_PlayerController::OnSetDestinationCanceled);
	}
}

void AInv_PlayerController::ToggleInventory()
{
	if (!InventoryComponent.IsValid()) return;

	InventoryComponent->ToggleInventoryMenu();

	if (InventoryComponent->IsMenuOpen())
	{
		if (IsValid(HUDWidget)) HUDWidget->SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		if (IsValid(HUDWidget)) HUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void AInv_PlayerController::CloseExternalInventory()
{
	if (!InventoryComponent.IsValid()) return;

	InventoryComponent->CloseExternalInventoryUI();

	// HUD visibility depends ONLY on main inventory
	if (!InventoryComponent->IsMenuOpen())
	{
		if (IsValid(HUDWidget))
		{
			HUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void AInv_PlayerController::Server_StartTrade_Implementation(AActor* MerchantActor)
{
	UE_LOG(LogTemp, Warning, TEXT("Server_StartTrade"));
	if (!IsValid(MerchantActor)) return;

	UInv_InventoryComponent* MerchantInv = MerchantActor->FindComponentByClass<UInv_InventoryComponent>();
	if (!IsValid(MerchantInv)) return;

	UE_LOG(LogTemp, Warning, TEXT("MerchantInv flags: bInitialized=%d Owner=%s NetMode=%d"),
	(int32)MerchantInv->bInitialized,
	*GetNameSafe(MerchantInv->GetOwner()),
	(int32)(MerchantInv->GetOwner() ? MerchantInv->GetOwner()->GetNetMode() : NM_MAX)
);

	Client_OpenMerchantTrade(MerchantActor);
	UE_LOG(LogTemp, Warning, TEXT("Server_StartTrade completed"));
}


void AInv_PlayerController::Client_OpenMerchantTrade_Implementation(AActor* Merchant)
{
	UE_LOG(LogTemp, Warning, TEXT("Client_OpenMerchantTrade completed"));
	if (!IsValid(Merchant) || !InventoryComponent.IsValid()) return;

	UInv_InventoryComponent* MerchantInv = Merchant->FindComponentByClass<UInv_InventoryComponent>();
	if (!IsValid(MerchantInv)) return;

	// This opens player inventory menu + external menu bound to merchant inv
	InventoryComponent->OpenExternalInventoryUI(MerchantInv);
}

void AInv_PlayerController::PrimaryInteract()
{
	TryPrimaryPickup(ThisActor.Get());
}

bool AInv_PlayerController::TryPrimaryPickup(AActor* TargetActor)
{
	if (!IsValid(TargetActor)) return false;

	UInv_ItemComponent* ItemComp = TargetActor->FindComponentByClass<UInv_ItemComponent>();
	if (!IsValid(ItemComp) || !InventoryComponent.IsValid()) return false;

	APawn* P = GetPawn();
	if (!IsValid(P)) return false;

	if (!IsWithinPickupDistance(TargetActor))
	{
		StartMoveToPickup(TargetActor, ItemComp);
		return true;
	}

	InventoryComponent->TryAddItem(ItemComp);
	return true;
}

void AInv_PlayerController::CreateHUDWidget()
{
	if (!IsLocalController()) return;

	HUDWidget = CreateWidget<UInv_HUDWidget>(this, HUDWidgetClass);
	if (IsValid(HUDWidget))
	{
		HUDWidget->AddToViewport();
	}
}

void AInv_PlayerController::TraceForItem()
{
	if (!IsValid(GEngine) || !IsValid(GEngine->GameViewport)) return;

	FVector2D ViewportSize;
	GEngine->GameViewport->GetViewportSize(ViewportSize);
	const FVector2D ViewportCenter = ViewportSize / 2.f;

	FVector TraceStart;
	FVector Forward;
	if (!UGameplayStatics::DeprojectScreenToWorld(this, ViewportCenter, TraceStart, Forward)) return;

	const FVector TraceEnd = TraceStart + Forward * TraceLength;
	FHitResult HitResult;
	GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ItemTraceChannel);

	LastActor = ThisActor;
	ThisActor = HitResult.GetActor();

	if (!ThisActor.IsValid())
	{
		if (IsValid(HUDWidget)) HUDWidget->HidePickupMessage();
	}

	if (ThisActor == LastActor) return;

	if (ThisActor.IsValid())
	{
		if (UActorComponent* Highlightable =
				ThisActor->FindComponentByInterface(UInv_Highlightable::StaticClass());
			IsValid(Highlightable))
		{
			IInv_Highlightable::Execute_Highlight(Highlightable);
		}

		UInv_ItemComponent* ItemComponent = ThisActor->FindComponentByClass<UInv_ItemComponent>();
		if (IsValid(ItemComponent))
		{
			if (IsValid(HUDWidget)) HUDWidget->ShowPickupMessage(ItemComponent->GetPickupMessage());
		}
	}

	if (LastActor.IsValid())
	{
		if (UActorComponent* Highlightable =
				LastActor->FindComponentByInterface(UInv_Highlightable::StaticClass());
			IsValid(Highlightable))
		{
			IInv_Highlightable::Execute_UnHighlight(Highlightable);
		}
	}
}

void AInv_PlayerController::TraceUnderMouseForItem()
{
	if (!IsLocalController()) return;

	LastActor = ThisActor;
	ThisActor = nullptr;

	FHitResult Hit;

	// 1) Try pickup items first (your current behavior)
	const bool bHitItem = GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ItemTraceChannel),
		/*bTraceComplex*/ false,
		Hit
	);

	AActor* NewActor = bHitItem ? Hit.GetActor() : nullptr;

	// If the hit is not an item, fallback to interactables
	if (!IsValid(NewActor) || !IsValid(NewActor->FindComponentByClass<UInv_ItemComponent>()))
	{
		FHitResult HitInteract;

		const bool bHitInteract = GetHitResultUnderCursorByChannel(
			UEngineTypes::ConvertToTraceType(InteractTraceChannel),
			/*bTraceComplex*/ false,
			HitInteract
		);

		NewActor = bHitInteract ? HitInteract.GetActor() : nullptr;
	}

	ThisActor = NewActor;

	// Clear HUD if no hit
	if (!ThisActor.IsValid())
	{
		if (IsValid(HUDWidget)) HUDWidget->HidePickupMessage();
	}

	// If unchanged, stop
	if (ThisActor == LastActor) return;

	// Unhighlight previous
	if (LastActor.IsValid())
	{
		if (UActorComponent* Highlightable =
				LastActor->FindComponentByInterface(UInv_Highlightable::StaticClass());
			IsValid(Highlightable))
		{
			IInv_Highlightable::Execute_UnHighlight(Highlightable);
		}
	}

	// Highlight new + show message
	if (ThisActor.IsValid())
	{
		if (UActorComponent* Highlightable =
				ThisActor->FindComponentByInterface(UInv_Highlightable::StaticClass());
			IsValid(Highlightable))
		{
			IInv_Highlightable::Execute_Highlight(Highlightable);
		}

		// Item message
		if (UInv_ItemComponent* ItemComponent = ThisActor->FindComponentByClass<UInv_ItemComponent>())
		{
			if (IsValid(HUDWidget)) HUDWidget->ShowPickupMessage(ItemComponent->GetPickupMessage());
		}
		else
		{
			// Optional: show generic interact prompt if target supports interaction
			// (You can add a better UI message later.)
			if (IsValid(HUDWidget)) HUDWidget->HidePickupMessage();
		}
	}
}

void AInv_PlayerController::ClearPendingPickup()
{
	GetWorldTimerManager().ClearTimer(PendingPickupTimerHandle);
	PendingPickupActor = nullptr;
	PendingPickupItemComp = nullptr;
}

void AInv_PlayerController::StartMoveToPickup(AActor* TargetActor, UInv_ItemComponent* ItemComp)
{
	if (!IsValid(TargetActor) || !IsValid(ItemComp)) return;

	PendingPickupActor = TargetActor;
	PendingPickupItemComp = ItemComp;

	// Start moving to the item
	MoveTo(TargetActor->GetActorLocation());

	// Poll until in range (PlayerController + SimpleMoveTo has no completion callback)
	GetWorldTimerManager().ClearTimer(PendingPickupTimerHandle);
	GetWorldTimerManager().SetTimer(
		PendingPickupTimerHandle,
		this,
		&AInv_PlayerController::TickPendingPickup,
		0.05f,
		true
	);
}

void AInv_PlayerController::TickPendingPickup()
{
	if (!InventoryComponent.IsValid())
	{
		ClearPendingPickup();
		return;
	}

	APawn* P = GetPawn();
	AActor* Target = PendingPickupActor.Get();
	UInv_ItemComponent* ItemComp = PendingPickupItemComp.Get();

	if (!IsValid(P) || !IsValid(Target) || !IsValid(ItemComp))
	{
		ClearPendingPickup();
		return;
	}

	if (IsWithinPickupDistance(Target))
	{
		InventoryComponent->TryAddItem(ItemComp);
		ClearPendingPickup();
		TraceUnderMouseForItem();
	}
}

void AInv_PlayerController::OnSetDestinationStarted()
{
	// LMB is movement-only. Do NOT attempt pickup here.
	bBlockMoveThisClick = false;

	// Cancel any pending pickup started by RMB if you want LMB to override intent
	// (optional, but usually desired)
	ClearPendingPickup();

	StopMovement();

	PressStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	FVector Loc;
	if (GetLocationUnderCursor(Loc))
	{
		CachedDestination = Loc;
	}
}

void AInv_PlayerController::OnSetDestinationTriggered()
{
	if (bBlockMoveThisClick)
	{
		return;
	}

	FVector Loc;
	if (GetLocationUnderCursor(Loc))
	{
		CachedDestination = Loc;
		Follow(CachedDestination);
	}
}

void AInv_PlayerController::OnSetDestinationCompleted()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float HeldSeconds = static_cast<float>(Now - PressStartTimeSeconds);

	// short click -> MoveTo
	if (!bBlockMoveThisClick && HeldSeconds < PressedThreshold)
	{
		MoveTo(CachedDestination);
	}

	bBlockMoveThisClick = false;
}

void AInv_PlayerController::OnSetDestinationCanceled()
{
	bBlockMoveThisClick = false;
	StopMovement();
}

bool AInv_PlayerController::GetLocationUnderCursor(FVector& OutLocation) const
{
	FHitResult Hit;
	const bool bHit = GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		/*bTraceComplex*/ true,
		Hit
	);

	if (!bHit) return false;

	OutLocation = Hit.ImpactPoint;
	return true;
}

void AInv_PlayerController::Follow(const FVector& Location)
{
	APawn* P = GetPawn();
	if (!IsValid(P)) return;

	const FVector PawnLoc = P->GetActorLocation();
	const FVector Dir2D = FVector(Location.X - PawnLoc.X, Location.Y - PawnLoc.Y, 0.f).GetSafeNormal();

	P->AddMovementInput(Dir2D, 1.0f);
}

void AInv_PlayerController::MoveTo(const FVector& Location)
{
	UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Location);

	if (FXCursor)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			FXCursor,
			Location,
			FRotator::ZeroRotator,
			FVector(1.f),
			true,
			true
		);
	}
}

bool AInv_PlayerController::IsWithinPickupDistance(const AActor* TargetActor) const
{
	const APawn* P = GetPawn();
	if (!IsValid(P) || !IsValid(TargetActor)) return false;

	const float MaxDistSq = MaxPickupDistance * MaxPickupDistance;
	const float DistSq = FVector::DistSquared2D(
		P->GetActorLocation(),
		TargetActor->GetActorLocation()
	);

	return DistSq <= MaxDistSq;
}
