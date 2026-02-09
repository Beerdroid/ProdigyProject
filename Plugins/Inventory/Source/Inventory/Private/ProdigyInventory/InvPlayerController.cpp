// Inv_PlayerController.cpp
#include "ProdigyInventory/InvPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "NiagaraFunctionLibrary.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Blueprint/UserWidget.h"
#include "ProdigyInventory/InventoryComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "ProdigyInventory/InventoryItemDBProvider.h"
#include "ProdigyInventory/InvHighlightable.h"
#include "ProdigyInventory/ItemComponent.h"
#include "Widgets/ProdigyInventory/InventoryWidgetBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogInvPickup, Log, All);

#define INV_PICKUP_LOG(Verbosity, Fmt, ...) \
UE_LOG(LogInvPickup, Verbosity, TEXT("[PC:%s] " Fmt), *GetNameSafe(this), ##__VA_ARGS__)

enum class EWidgetSide : uint8
{
	None,
	Left,
	Right,
	Top,
	Bottom
};

static void ApplyTopDownMouseCaptureMode()
{
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetMouseLockMode(EMouseLockMode::DoNotLock);

		// This is the anti-double-click setting
		GEngine->GameViewport->SetMouseCaptureMode(
			EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown
		);

		GEngine->GameViewport->SetHideCursorDuringCapture(false);
	}
}

static FVector2D ClampWidgetPosToViewport(
	const FVector2D& DesiredPos,
	const FVector2D& WidgetSize,
	const FVector2D& ViewportSize,
	const FVector2D& PaddingPx)
{
	FVector2D P = DesiredPos;

	// clamp left/top
	P.X = FMath::Max(PaddingPx.X, P.X);
	P.Y = FMath::Max(PaddingPx.Y, P.Y);

	// clamp right/bottom
	P.X = FMath::Min(P.X, ViewportSize.X - WidgetSize.X - PaddingPx.X);
	P.Y = FMath::Min(P.Y, ViewportSize.Y - WidgetSize.Y - PaddingPx.Y);

	return P;
}

static void PlaceSingleWidgetRight(
	APlayerController* PC,
	UUserWidget* Widget,
	FVector2D PaddingPx = FVector2D(16.f, 16.f),
	bool bRemoveDPIScale = true)
{
	if (!IsValid(PC) || !IsValid(Widget)) return;

	int32 VX = 0, VY = 0;
	PC->GetViewportSize(VX, VY);
	if (VX <= 0 || VY <= 0) return;

	const FVector2D ViewportSize((float)VX, (float)VY);

	Widget->ForceLayoutPrepass();
	FVector2D Size = Widget->GetDesiredSize();
	if (Size.IsNearlyZero()) return;

	// Right-center
	const float CenterY = ViewportSize.Y * 0.5f;
	FVector2D Pos(ViewportSize.X - PaddingPx.X - Size.X, CenterY - Size.Y * 0.5f);

	Pos = ClampWidgetPosToViewport(Pos, Size, ViewportSize, PaddingPx);

	Widget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
	Widget->SetPositionInViewport(Pos, bRemoveDPIScale);
}

static void PlaceWidgetInViewport(
	APlayerController* PC,
	UUserWidget* LeftWidget, // external
	UUserWidget* RightWidget, // player
	float GapPx = 16.f,
	FVector2D PaddingPx = FVector2D(16.f, 16.f),
	bool bRemoveDPIScale = true
)
{
	if (!IsValid(PC) || !IsValid(LeftWidget) || !IsValid(RightWidget)) return;

	int32 VX = 0, VY = 0;
	PC->GetViewportSize(VX, VY);
	if (VX <= 0 || VY <= 0) return;

	const FVector2D ViewportSize((float)VX, (float)VY);

	LeftWidget->ForceLayoutPrepass();
	RightWidget->ForceLayoutPrepass();

	FVector2D LSize = LeftWidget->GetDesiredSize();
	FVector2D RSize = RightWidget->GetDesiredSize();

	// If desired size isn't ready, bail safely (UMG sometimes reports 0 on first frame)
	if (LSize.IsNearlyZero() || RSize.IsNearlyZero()) return;

	// Can we fit side-by-side?
	const float NeededWidth = PaddingPx.X + LSize.X + GapPx + RSize.X + PaddingPx.X;
	const bool bSideBySide = (ViewportSize.X >= NeededWidth);

	// Choose positions (top-left anchored coords)
	FVector2D LPos, RPos;

	if (bSideBySide)
	{
		// Side-by-side, centered vertically
		const float CenterY = ViewportSize.Y * 0.5f;

		LPos = FVector2D(PaddingPx.X, CenterY - LSize.Y * 0.5f);
		RPos = FVector2D(ViewportSize.X - PaddingPx.X - RSize.X, CenterY - RSize.Y * 0.5f);
	}
	else
	{
		// Stack vertically (external on top, player on bottom)
		// Center horizontally for each widget
		const float CenterX = ViewportSize.X * 0.5f;

		LPos = FVector2D(CenterX - LSize.X * 0.5f, PaddingPx.Y);
		RPos = FVector2D(CenterX - RSize.X * 0.5f, ViewportSize.Y - PaddingPx.Y - RSize.Y);
	}

	// Clamp to viewport
	LPos = ClampWidgetPosToViewport(LPos, LSize, ViewportSize, PaddingPx);
	RPos = ClampWidgetPosToViewport(RPos, RSize, ViewportSize, PaddingPx);

	// Apply in viewport (top-left alignment)
	LeftWidget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
	RightWidget->SetAlignmentInViewport(FVector2D(0.f, 0.f));

	LeftWidget->SetPositionInViewport(LPos, bRemoveDPIScale);
	RightWidget->SetPositionInViewport(RPos, bRemoveDPIScale);
}

void AInvPlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsurePlayerInventoryResolved();

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem))
	{
		Subsystem->AddMappingContext(DefaultIMC, 0);
	}

	InventoryComponent = FindComponentByClass<UInventoryComponent>();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	ApplyTopDownMouseCaptureMode();

	// Hover highlight + pickup message
	GetWorldTimerManager().SetTimer(
		TraceTimerHandle,
		this,
		&AInvPlayerController::TraceUnderMouseForItem,
		0.05f,
		true
	);
}

void AInvPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UE_LOG(LogTemp, Warning, TEXT("SetupInputComponent called on %s (Class=%s)"),
	       *GetNameSafe(this), *GetClass()->GetName());

	UEnhancedInputComponent* EnhancedInputComponent =
		CastChecked<UEnhancedInputComponent>(InputComponent);

	EnhancedInputComponent->BindAction(
		PrimaryInteractAction, ETriggerEvent::Started, this, &AInvPlayerController::PrimaryInteract);
	EnhancedInputComponent->BindAction(
		ToggleInventoryAction, ETriggerEvent::Started, this, &AInvPlayerController::ToggleInventory);

	// TopDown click/hold movement
	if (SetDestinationClickAction)
	{
		EnhancedInputComponent->BindAction(
			SetDestinationClickAction, ETriggerEvent::Started, this, &AInvPlayerController::OnSetDestinationStarted);
		EnhancedInputComponent->BindAction(
			SetDestinationClickAction, ETriggerEvent::Triggered, this,
			&AInvPlayerController::OnSetDestinationTriggered);
		EnhancedInputComponent->BindAction(
			SetDestinationClickAction, ETriggerEvent::Completed, this,
			&AInvPlayerController::OnSetDestinationCompleted);
		EnhancedInputComponent->BindAction(
			SetDestinationClickAction, ETriggerEvent::Canceled, this, &AInvPlayerController::OnSetDestinationCanceled);
	}
}


void AInvPlayerController::EnsurePlayerInventoryResolved()
{
	if (InventoryComponent.IsValid()) return;

	APawn* P = GetPawn();
	if (!IsValid(P)) return;

	// If inventory is added on the Pawn instead of Controller, you can also search there.
	// For your plan (inventory comp added to the controller BP), this will usually find it:
	InventoryComponent = FindComponentByClass<UInventoryComponent>();
	if (!InventoryComponent.IsValid())
	{
		// fallback: maybe it's on Pawn
		InventoryComponent = P->FindComponentByClass<UInventoryComponent>();
	}

	INV_PICKUP_LOG(Log, TEXT("EnsurePlayerInventoryResolved: Resolved=%s (OnPC=%d OnPawn=%d)"),
	               *GetNameSafe(InventoryComponent.Get()),
	               FindComponentByClass<UInventoryComponent>() != nullptr ? 1 : 0,
	               P->FindComponentByClass<UInventoryComponent>() != nullptr ? 1 : 0);
}

void AInvPlayerController::AddGold(int32 Delta)
{
	const int32 Old = Gold;
	Gold = FMath::Max(0, Gold + Delta);

	if (Gold != Old)
	{
		OnGoldChanged.Broadcast(Gold);
	}
}


void AInvPlayerController::SetGold(int32 NewGold)
{
	NewGold = FMath::Max(0, NewGold);
	if (Gold == NewGold) return;

	Gold = NewGold;
	OnGoldChanged.Broadcast(Gold);
}

bool AInvPlayerController::TryGetItemPrice(FName ItemID, int32& OutSellValue) const
{
	OutSellValue = 0;
	if (!InventoryComponent.IsValid() || ItemID.IsNone()) return false;

	FItemRow Row;
	if (!InventoryComponent->TryGetItemDef(ItemID, Row)) return false;

	OutSellValue = FMath::Max(0, Row.SellValue);
	return true;
}

void AInvPlayerController::SetExternalInventory(UInventoryComponent* NewExternal)
{
	ExternalInventory = NewExternal;
	OnExternalInventoryChanged.Broadcast(ExternalInventory.Get());
}

void AInvPlayerController::ClearExternalInventory()
{
	ExternalInventory = nullptr;
	OnExternalInventoryChanged.Broadcast(nullptr);
}

bool AInvPlayerController::SetExternalInventoryFromActor(AActor* ActorWithInventory)
{
	if (!IsValid(ActorWithInventory)) return false;

	UInventoryComponent* Inv = ActorWithInventory->FindComponentByClass<UInventoryComponent>();
	SetExternalInventory(Inv);
	return IsValid(Inv);
}

bool AInvPlayerController::Player_MoveOrSwap(int32 FromIndex, int32 ToIndex, TArray<int32>& OutChanged)
{
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid()) return false;
	return InventoryComponent->MoveOrSwap(FromIndex, ToIndex, OutChanged);
}

bool AInvPlayerController::Player_SplitStack(int32 FromIndex, int32 ToIndex, int32 SplitAmount,
                                             TArray<int32>& OutChanged)
{
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid()) return false;
	return InventoryComponent->SplitStack(FromIndex, ToIndex, SplitAmount, OutChanged);
}

void AInvPlayerController::GetDropTransform(FVector& OutLoc, FRotator& OutRot) const
{
	OutRot = FRotator::ZeroRotator;

	APawn* P = GetPawn();
	if (!IsValid(P))
	{
		OutLoc = FVector::ZeroVector;
		return;
	}

	const FVector PawnLoc = P->GetActorLocation();

	// Random angle around pawn
	const float AngleRad = FMath::FRandRange(0.f, 2.f * PI);

	// Random radius (sqrt for uniform distribution in a circle)
	const float Radius = FMath::Sqrt(FMath::FRand()) *
		FMath::FRandRange(DropMinRadius, DropMaxRadius);

	const FVector Offset(
		FMath::Cos(AngleRad) * Radius,
		FMath::Sin(AngleRad) * Radius,
		0.f
	);

	FVector Desired = PawnLoc + Offset;

	// Ground snap
	FHitResult Hit;
	const FVector TraceStart = Desired + FVector(0.f, 0.f, DropHeightTrace);
	const FVector TraceEnd = Desired - FVector(0.f, 0.f, DropHeightTrace);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DropTrace), false, P);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		Params
	);

	OutLoc = bHit ? Hit.ImpactPoint : Desired;

	// Optional: rotate pickup to face camera yaw or random yaw
	OutRot = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f);
}

bool AInvPlayerController::Player_DropFromSlot(int32 SlotIndex, int32 Quantity, TArray<int32>& OutChanged)
{
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid()) return false;

	FVector Loc;
	FRotator Rot;
	GetDropTransform(Loc, Rot);

	return InventoryComponent->DropFromSlot(SlotIndex, Quantity, Loc, Rot, OutChanged);
}

bool AInvPlayerController::Transfer_PlayerToExternal(int32 PlayerFromIndex, int32 ExternalToIndex, int32 Quantity,
                                                     TArray<int32>& OutChangedPlayer, TArray<int32>& OutChangedExternal)
{
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid() || !ExternalInventory.IsValid()) return false;

	// Source = player, target = external
	return InventoryComponent->TransferTo(ExternalInventory.Get(), PlayerFromIndex, ExternalToIndex, Quantity,
	                                      OutChangedPlayer, OutChangedExternal);
}

bool AInvPlayerController::Transfer_ExternalToPlayer(int32 ExternalFromIndex, int32 PlayerToIndex, int32 Quantity,
                                                     TArray<int32>& OutChangedExternal, TArray<int32>& OutChangedPlayer)
{
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid() || !ExternalInventory.IsValid()) return false;

	// Source = external, target = player
	return ExternalInventory->TransferTo(
		InventoryComponent.Get(),
		ExternalFromIndex,
		PlayerToIndex,
		Quantity,
		OutChangedExternal,
		OutChangedPlayer
	);
}

bool AInvPlayerController::AutoTransfer_PlayerToExternal(int32 PlayerFromIndex, int32 Quantity,
                                                         TArray<int32>& OutChangedPlayer,
                                                         TArray<int32>& OutChangedExternal)
{
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid() || !ExternalInventory.IsValid()) return false;

	return InventoryComponent->AutoTransferTo(ExternalInventory.Get(), PlayerFromIndex, Quantity, OutChangedPlayer,
	                                          OutChangedExternal);
}

bool AInvPlayerController::AutoTransfer_ExternalToPlayer(int32 ExternalFromIndex, int32 Quantity,
                                                         TArray<int32>& OutChangedExternal,
                                                         TArray<int32>& OutChangedPlayer)
{
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid() || !ExternalInventory.IsValid()) return false;

	return ExternalInventory->AutoTransferTo(
		InventoryComponent.Get(),
		ExternalFromIndex,
		Quantity,
		OutChangedExternal,
		OutChangedPlayer
	);
}

bool AInvPlayerController::Player_ConsumeOne(int32 SlotIndex, TArray<int32>& OutChanged)
{
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid()) return false;

	// Minimal “consume” = just remove 1. Real effect application should be elsewhere.
	return InventoryComponent->RemoveFromSlot(SlotIndex, 1, OutChanged);
}

bool AInvPlayerController::IsWithinPickupDistance(const AActor* TargetActor) const
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


void AInvPlayerController::PrimaryInteract()
{
	TryPrimaryPickup(ThisActor.Get());
}

bool AInvPlayerController::TryPrimaryPickup(AActor* TargetActor)
{
	if (!IsValid(TargetActor)) return false;

	UItemComponent* ItemComp = TargetActor->FindComponentByClass<UItemComponent>();
	if (!IsValid(ItemComp) || !InventoryComponent.IsValid()) return false;

	APawn* P = GetPawn();
	if (!IsValid(P)) return false;

	INV_PICKUP_LOG(Log, TEXT("TryPrimaryPickup: Target=%s ItemID=%s Qty=%d InRange=%d"),
	               *GetNameSafe(TargetActor),
	               *ItemComp->ItemID.ToString(),
	               ItemComp->Quantity,
	               IsWithinPickupDistance(TargetActor) ? 1 : 0);

	if (!IsWithinPickupDistance(TargetActor))
	{
		StartMoveToPickup(TargetActor, ItemComp);
		return true;
	}

	InventoryComponent->TryPickupFromItemComponent_FullOnly(ItemComp);
	return true;
}

void AInvPlayerController::TraceForItem()
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


	if (ThisActor == LastActor) return;

	if (ThisActor.IsValid())
	{
		if (UActorComponent* Highlightable =
				ThisActor->FindComponentByInterface(UInvHighlightable::StaticClass());
			IsValid(Highlightable))
		{
			IInvHighlightable::Execute_Highlight(Highlightable);
		}
	}

	if (LastActor.IsValid())
	{
		if (UActorComponent* Highlightable =
				LastActor->FindComponentByInterface(UInvHighlightable::StaticClass());
			IsValid(Highlightable))
		{
			IInvHighlightable::Execute_UnHighlight(Highlightable);
		}
	}
}

void AInvPlayerController::TraceUnderMouseForItem()
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
	if (!IsValid(NewActor) || !IsValid(NewActor->FindComponentByClass<UItemComponent>()))
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

	// If unchanged, stop
	if (ThisActor == LastActor) return;

	// Unhighlight previous
	if (LastActor.IsValid())
	{
		if (UActorComponent* Highlightable =
				LastActor->FindComponentByInterface(UInvHighlightable::StaticClass());
			IsValid(Highlightable))
		{
			IInvHighlightable::Execute_UnHighlight(Highlightable);
		}
	}

	// Highlight new + show message
	if (ThisActor.IsValid())
	{
		if (UActorComponent* Highlightable =
				ThisActor->FindComponentByInterface(UInvHighlightable::StaticClass());
			IsValid(Highlightable))
		{
			IInvHighlightable::Execute_Highlight(Highlightable);
		}
	}
	const bool bIsItem = ThisActor.IsValid() && ThisActor->FindComponentByClass<UItemComponent>() != nullptr;

	INV_PICKUP_LOG(Verbose, TEXT("HoverChanged: New=%s IsItem=%d Last=%s"),
	               *GetNameSafe(ThisActor.Get()),
	               bIsItem ? 1 : 0,
	               *GetNameSafe(LastActor.Get()));
}

void AInvPlayerController::ClearPendingPickup()
{
	if (PendingPickupActor.IsValid() || PendingPickupItemComp.IsValid())
	{
		INV_PICKUP_LOG(Verbose, TEXT("ClearPendingPickup: Target=%s ItemCompOwner=%s"),
		               *GetNameSafe(PendingPickupActor.Get()),
		               *GetNameSafe(PendingPickupItemComp.IsValid() ? PendingPickupItemComp->GetOwner() : nullptr));
	}

	GetWorldTimerManager().ClearTimer(PendingPickupTimerHandle);
	PendingPickupActor = nullptr;
	PendingPickupItemComp = nullptr;
}

void AInvPlayerController::StartMoveToPickup(AActor* TargetActor, UItemComponent* ItemComp)
{
	if (!IsValid(TargetActor) || !IsValid(ItemComp)) return;

	INV_PICKUP_LOG(Log, TEXT("StartMoveToPickup: Target=%s ItemID=%s Qty=%d"),
	               *GetNameSafe(TargetActor),
	               *ItemComp->ItemID.ToString(),
	               ItemComp->Quantity);

	PendingPickupActor = TargetActor;
	PendingPickupItemComp = ItemComp;

	// Start moving to the item
	MoveTo(TargetActor->GetActorLocation());

	// Poll until in range (PlayerController + SimpleMoveTo has no completion callback)
	GetWorldTimerManager().ClearTimer(PendingPickupTimerHandle);
	GetWorldTimerManager().SetTimer(
		PendingPickupTimerHandle,
		this,
		&AInvPlayerController::TickPendingPickup,
		0.05f,
		true
	);
}

void AInvPlayerController::TickPendingPickup()
{
	if (!InventoryComponent.IsValid())
	{
		ClearPendingPickup();
		return;
	}

	APawn* P = GetPawn();
	AActor* Target = PendingPickupActor.Get();
	UItemComponent* ItemComp = PendingPickupItemComp.Get();

	if (!IsValid(P) || !IsValid(Target) || !IsValid(ItemComp))
	{
		ClearPendingPickup();
		return;
	}

	const bool bInRange = IsWithinPickupDistance(Target);


	INV_PICKUP_LOG(Verbose, TEXT("TickPendingPickup: Target=%s InRange=%d ItemID=%s Qty=%d"),
	               *GetNameSafe(Target),
	               bInRange ? 1 : 0,
	               *ItemComp->ItemID.ToString(),
	               ItemComp->Quantity);

	if (IsWithinPickupDistance(Target))
	{
		InventoryComponent->TryPickupFromItemComponent_FullOnly(ItemComp);
		ClearPendingPickup();
		TraceUnderMouseForItem();
	}
}

void AInvPlayerController::OnSetDestinationStarted()
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

void AInvPlayerController::OnSetDestinationTriggered()
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

void AInvPlayerController::OnSetDestinationCompleted()
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float HeldSeconds = static_cast<float>(Now - PressStartTimeSeconds);

	// short click -> MoveTo
	if (!bBlockMoveThisClick && HeldSeconds < PressedThreshold)
	{
		MoveTo(CachedDestination);
	}

	bBlockMoveThisClick = false;

	if (!bBlockMoveThisClick && HeldSeconds < PressedThreshold)
	{
		AActor* Clicked = GetActorUnderCursorForClick(); // plugin-local helper; no game deps
		if (HandlePrimaryClickActor(Clicked))
		{
			bBlockMoveThisClick = true;
		}

		if (!bBlockMoveThisClick)
		{
			MoveTo(CachedDestination);
		}
	}
}

void AInvPlayerController::OnSetDestinationCanceled()
{
	bBlockMoveThisClick = false;
	StopMovement();
}

bool AInvPlayerController::GetLocationUnderCursor(FVector& OutLocation) const
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

void AInvPlayerController::Follow(const FVector& Location)
{
	APawn* P = GetPawn();
	if (!IsValid(P)) return;

	const FVector PawnLoc = P->GetActorLocation();
	const FVector Dir2D = FVector(Location.X - PawnLoc.X, Location.Y - PawnLoc.Y, 0.f).GetSafeNormal();

	P->AddMovementInput(Dir2D, 1.0f);
}

void AInvPlayerController::MoveTo(const FVector& Location)
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

void AInvPlayerController::SetInventory(UInventoryComponent* InInventory)
{
	// UI binding is only meaningful on owning client
	const bool bIsLocal = IsLocalController();

	// If not provided, resolve from THIS controller (since your BP adds it to the PC)
	UInventoryComponent* Resolved = InInventory;
	if (!IsValid(Resolved))
	{
		Resolved = FindComponentByClass<UInventoryComponent>();
	}

	// Clear case
	if (!IsValid(Resolved))
	{
		InventoryComponent = nullptr;

		if (bIsLocal)
		{
			if (UInventoryWidgetBase* W = Cast<UInventoryWidgetBase>(PlayerInventoryWidget))
			{
				W->SetInventory(nullptr);
			}
		}
		return;
	}

	// Set (even if same, we may want to rebind UI)
	InventoryComponent = Resolved;

	// If widget already exists, bind immediately
	if (bIsLocal)
	{
		if (UInventoryWidgetBase* W = Cast<UInventoryWidgetBase>(PlayerInventoryWidget))
		{
			W->SetInventory(InventoryComponent.Get());
		}
	}
}

void AInvPlayerController::OpenInventory()
{
	if (bInventoryOpen) return;

	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid()) return;

	if (!IsValid(PlayerInventoryWidget))
	{
		PlayerInventoryWidget = CreateWidget<UUserWidget>(this, PlayerInventoryWidgetClass);
	}
	if (!IsValid(PlayerInventoryWidget)) return;

	if (UInventoryWidgetBase* W = Cast<UInventoryWidgetBase>(PlayerInventoryWidget))
	{
		W->SetInventory(InventoryComponent.Get());
	}

	PlayerInventoryWidget->AddToViewport(1000);

	// Place right-center next tick (as you have)
	GetWorldTimerManager().SetTimerForNextTick([this]()
	{
		if (!IsValid(PlayerInventoryWidget)) return;

		// If external is valid + in viewport -> place both
		if (IsValid(ExternalInventoryWidget) && ExternalInventoryWidget->IsInViewport())
		{
			PlaceWidgetInViewport(
				this,
				ExternalInventoryWidget,
				PlayerInventoryWidget,
				/*GapPx*/ 16.f,
				/*PaddingPx*/ FVector2D(16.f, 16.f),
				/*bRemoveDPIScale*/ true
			);
		}
		else
		{
			// Player-only case
			PlaceSingleWidgetRight(
				this,
				PlayerInventoryWidget,
				/*PaddingPx*/ FVector2D(16.f, 16.f),
				/*bRemoveDPIScale*/ true
			);
		}
	});

	// Game + UI, but don't force focus to the widget
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	// While open, you can keep "no capture" too (prevents weirdness with drag)
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
		GEngine->GameViewport->SetHideCursorDuringCapture(false);
	}

	bInventoryOpen = true;
}

void AInvPlayerController::CloseInventory()
{
	if (!bInventoryOpen) return;

	if (IsValid(PlayerInventoryWidget))
	{
		PlayerInventoryWidget->RemoveFromParent();
	}
	if (IsValid(ExternalInventoryWidget))
	{
		ExternalInventoryWidget->RemoveFromParent();
	}

	ExternalInventory = nullptr;

	// Game-only but do not eat the click that triggers capture
	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	SetInputMode(InputMode);

	FlushPressedKeys();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	// Re-apply viewport capture behavior (prevents drift after UI)
	ApplyTopDownMouseCaptureMode();

	bBlockMoveThisClick = false;
	PressStartTimeSeconds = 0.0;

	bInventoryOpen = false;

	if (UInventoryWidgetBase* W = Cast<UInventoryWidgetBase>(PlayerInventoryWidget))
	{
		W->CancelSplitMode();
		W->CloseContextMenu();
		W->HideTooltip();
	}
}

void AInvPlayerController::ToggleInventory()
{
	if (bInventoryOpen)
	{
		CloseInventory();
	}
	else
	{
		OpenInventory();
	}
}

bool AInvPlayerController::CanUseExternalInventory(const UInventoryComponent* External,
                                                   const AActor* ExternalOwner) const
{
	if (!IsValid(External) || !IsValid(ExternalOwner)) return false;

	// Optional: forbid opening your own inventory as external
	if (External == InventoryComponent.Get()) return false;

	// Optional: distance gate (use same pattern as pickup)
	const APawn* P = GetPawn();
	if (!IsValid(P)) return false;

	const float MaxDistSq = MaxInteractDistance * MaxInteractDistance; // add MaxInteractDistance
	const float DistSq = FVector::DistSquared2D(P->GetActorLocation(), ExternalOwner->GetActorLocation());
	if (DistSq > MaxDistSq) return false;

	return true;
}

void AInvPlayerController::OpenExternalInventoryFromActor(AActor* ActorWithInventory)
{
	if (!IsLocalController()) return;

	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid()) return;
	if (!IsValid(ActorWithInventory)) return;

	// Resolve external inventory component
	UInventoryComponent* Ext = ActorWithInventory->FindComponentByClass<UInventoryComponent>();
	if (!IsValid(Ext)) return;

	// Ready checks (distance etc.)
	if (!CanUseExternalInventory(Ext, ActorWithInventory)) return;

	// Set external reference + notify UI listeners
	SetExternalInventory(Ext);

	// Ensure player inventory is open (common UX)
	if (!bInventoryOpen)
	{
		OpenInventory();
	}

	// Create external widget lazily
	if (!IsValid(ExternalInventoryWidget))
	{
		ExternalInventoryWidget = CreateWidget<UUserWidget>(this, ExternalInventoryWidgetClass);
	}
	if (!IsValid(ExternalInventoryWidget)) return;

	// Bind inventory into widget
	if (UInventoryWidgetBase* W = Cast<UInventoryWidgetBase>(ExternalInventoryWidget))
	{
		W->SetInventory(ExternalInventory.Get());
	}

	ExternalInventoryWidget->AddToViewport(1000);

	// Place left-center next tick (matching your player inventory placement style)
	GetWorldTimerManager().SetTimerForNextTick([this]()
	{
		if (!IsValid(ExternalInventoryWidget)) return;

		// If player inventory is open too, place both
		if (IsValid(PlayerInventoryWidget))
		{
			PlaceWidgetInViewport(
				this,
				ExternalInventoryWidget,
				PlayerInventoryWidget,
				/*GapPx*/ 16.f,
				/*PaddingPx*/ FVector2D(16.f, 16.f),
				/*bRemoveDPIScale*/ true
			);
		}
		else
		{
			// If somehow external opens alone, place it left-center
			ExternalInventoryWidget->SetAlignmentInViewport(FVector2D(0.f, 0.5f));
			ExternalInventoryWidget->SetPositionInViewport(FVector2D(40.f, 0.f), /*bRemoveDPIScale*/ true);
		}
	});

	// Keep the same UI input mode behavior as inventory open
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	// Prevent capture weirdness while UI is visible
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
		GEngine->GameViewport->SetHideCursorDuringCapture(false);
	}
}

void AInvPlayerController::CloseExternalInventory()
{
	if (!IsLocalController()) return;

	if (IsValid(ExternalInventoryWidget))
	{
		ExternalInventoryWidget->RemoveFromParent();
	}

	// Unbind pointer + notify listeners
	ClearExternalInventory();

	// If your UX is “external closes but player stays open”, do nothing else.
	// If you want closing external to also close player inventory, call CloseInventory() here.
}

bool AInvPlayerController::ExecuteInventoryAction(UInventoryComponent* Source, int32 SourceIndex,
                                                  UInventoryComponent* Target, int32 TargetIndex, int32 Quantity,
                                                  EInvAction Action, bool bAllowSwap, bool bFullOnly)
{
	// --------- Validate ----------
	if (!IsValid(Source) || !IsValid(Target)) return false;
	if (!Source->IsValidIndex(SourceIndex)) return false;

	const FInventorySlot From = Source->GetSlot(SourceIndex);
	if (From.IsEmpty()) return false;

	const int32 Requested = (Quantity <= 0) ? From.Quantity : FMath::Min(Quantity, From.Quantity);
	if (Requested <= 0) return false;

	const FName TradedItemID = From.ItemID;

	const EInventoryType SrcType = Source->InventoryType;
	const EInventoryType DstType = Target->InventoryType;

	const bool bPlayerToMerchant = (SrcType == EInventoryType::Player && DstType == EInventoryType::Merchant);
	const bool bMerchantToPlayer = (SrcType == EInventoryType::Merchant && DstType == EInventoryType::Player);
	const bool bIsMerchantTrade = bPlayerToMerchant || bMerchantToPlayer;

	// Optional: forbid swap for merchant trades if requested
	if (bIsMerchantTrade && !bAllowSwap)
	{
		// If we are going to use DragDrop -> TransferTo, validate slot policy now.
		// For AutoTransferTo we don't target a slot, and it never swaps anyway.
		if (Action == EInvAction::DragDrop)
		{
			if (!Target->IsValidIndex(TargetIndex)) return false;

			const FInventorySlot To = Target->GetSlot(TargetIndex);
			if (!To.IsEmpty() && To.ItemID != TradedItemID)
			{
				return false; // disallow swapping different items in merchant trade
			}
		}
	}

	// Optional: full-only policy (useful for "buy must fully fit")
	if (bFullOnly)
	{
		if (!Target->CanFullyAdd(TradedItemID, Requested))
		{
			return false;
		}
	}

	// --------- Price precheck (BEFORE move) ----------
	int32 UnitPrice = 0;

	if (bIsMerchantTrade)
	{
		// Base price from DB: Row.SellValue
		if (!TryGetItemPrice(TradedItemID, UnitPrice))
		{
			return false; // can't price -> block trade
		}

		// For now: buy price == sell value (later: merchant coefs)
		if (bMerchantToPlayer)
		{
			const int32 TotalCost = UnitPrice * Requested;
			if (!CanAfford(TotalCost))
			{
				return false; // can't afford -> do not move item
			}
		}
	}

	// --------- Execute move ----------
	bool bOk = false;
	TArray<int32> ChangedS, ChangedT;

	// Merchant / quick / auto => stack-first, no explicit target slot
	const bool bUseAuto =
		bIsMerchantTrade ||
		Action == EInvAction::AutoMove ||
		Action == EInvAction::QuickUse;

	if (bUseAuto)
	{
		bOk = Source->AutoTransferTo(Target, SourceIndex, Requested, ChangedS, ChangedT);
	}
	else
	{
		if (!Target->IsValidIndex(TargetIndex)) return false;
		bOk = Source->TransferTo(Target, SourceIndex, TargetIndex, Requested, ChangedS, ChangedT);
	}

	if (!bOk) return false;

	// --------- Apply gold (AFTER move success) ----------
	if (bIsMerchantTrade)
	{
		const int32 Total = UnitPrice * Requested;

		if (bMerchantToPlayer)
		{
			AddGold(-Total); // BUY
		}
		else if (bPlayerToMerchant)
		{
			AddGold(+Total); // SELL
		}
	}

	return true;
}

AActor* AInvPlayerController::GetActorUnderCursorForClick() const
{
	FHitResult Hit;

	// Prefer interact trace for characters/NPCs/world, because ItemTraceChannel is tuned for pickups.
	const bool bHitInteract = GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(InteractTraceChannel),
		false,
		Hit
	);

	if (bHitInteract && Hit.GetActor()) return Hit.GetActor();

	// Fallback to item trace
	const bool bHitItem = GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ItemTraceChannel),
		false,
		Hit
	);

	return bHitItem ? Hit.GetActor() : nullptr;
}
