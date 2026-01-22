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

	UEnhancedInputComponent* EnhancedInputComponent =
		CastChecked<UEnhancedInputComponent>(InputComponent);

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

void AInv_PlayerController::PrimaryInteract()
{
	if (!ThisActor.IsValid()) return;

	UInv_ItemComponent* ItemComp = ThisActor->FindComponentByClass<UInv_ItemComponent>();
	if (!IsValid(ItemComp) || !InventoryComponent.IsValid()) return;

	APawn* P = GetPawn();
	if (!IsValid(P)) return;

	if (!IsWithinPickupDistance(ThisActor.Get()))
	{
		StartMoveToPickup(ThisActor.Get(), ItemComp);
		return;
	}

	InventoryComponent->TryAddItem(ItemComp);
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

	FHitResult HitResult;
	const bool bHit = GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ItemTraceChannel),
		/*bTraceComplex*/ false,
		HitResult
	);

	LastActor = ThisActor;
	ThisActor = bHit ? HitResult.GetActor() : nullptr;

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

		UInv_ItemComponent* ItemComponent = ThisActor->FindComponentByClass<UInv_ItemComponent>();
		if (IsValid(ItemComponent))
		{
			if (IsValid(HUDWidget)) HUDWidget->ShowPickupMessage(ItemComponent->GetPickupMessage());
		}
		else
		{
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

bool AInv_PlayerController::TryPickupItemUnderMouse_Refresh()
{
	if (!IsLocalController()) return false;
	if (!InventoryComponent.IsValid()) return false;

	ClearPendingPickup();

	FHitResult HitResult;
	const bool bHit = GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ItemTraceChannel),
		/*bTraceComplex*/ false,
		HitResult
	);

	if (!bHit) return false;

	AActor* HitActor = HitResult.GetActor();
	if (!IsValid(HitActor)) return false;

	UInv_ItemComponent* ItemComp = HitActor->FindComponentByClass<UInv_ItemComponent>();
	if (!IsValid(ItemComp)) return false;

	APawn* P = GetPawn();
	if (!IsValid(P)) return false;

	if (IsWithinPickupDistance(HitActor))
	{
		InventoryComponent->TryAddItem(ItemComp);
		return true;
	}

	// Out of range: move to item and pickup on arrival
	StartMoveToPickup(HitActor, ItemComp);
	return true;
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
	// Pickup-first: if click was on item, handle pickup or move-to-pickup and block normal movement click
	bBlockMoveThisClick = TryPickupItemUnderMouse_Refresh();
	if (bBlockMoveThisClick)
	{
		// IMPORTANT:
		// If we started "move-to-pickup", do NOT StopMovement(), or you'll cancel SimpleMoveToLocation.
		// Only stop movement when we actually picked up immediately (no pending target).
		if (!PendingPickupActor.IsValid())
		{
			StopMovement();
		}

		TraceUnderMouseForItem();
		return;
	}

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
