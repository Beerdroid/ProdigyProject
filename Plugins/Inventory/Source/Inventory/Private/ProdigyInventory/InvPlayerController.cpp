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

static bool PlaceWidgetInViewport(
	UUserWidget* Widget,
	APlayerController* PC,
	const FVector2D& ViewportAnchor = FVector2D(0.5f, 0.5f),
	const FVector2D& Alignment      = FVector2D(0.5f, 0.5f),
	const FVector2D& AnchorOffsetPx = FVector2D::ZeroVector,
	UUserWidget* RelativeTo         = nullptr,
	EWidgetSide Side                = EWidgetSide::None,
	float GapPx                     = 0.f,
	const FVector2D& ForcedSize     = FVector2D::ZeroVector,
	bool bRemoveDPIScale            = true
)
{
	if (!IsValid(Widget) || !IsValid(PC)) return false;

	if (!ForcedSize.IsNearlyZero())
	{
		Widget->SetDesiredSizeInViewport(ForcedSize);
	}

	int32 VX = 0, VY = 0;
	PC->GetViewportSize(VX, VY);
	if (VX <= 0 || VY <= 0) return false;

	Widget->ForceLayoutPrepass();
	if (RelativeTo) RelativeTo->ForceLayoutPrepass();

	const FVector2D ViewportSize((float)VX, (float)VY);
	const FVector2D AnchorPos(ViewportSize.X * ViewportAnchor.X, ViewportSize.Y * ViewportAnchor.Y);

	FVector2D ThisSize = Widget->GetDesiredSize();
	if (ThisSize.IsNearlyZero() && !ForcedSize.IsNearlyZero())
	{
		ThisSize = ForcedSize;
	}

	Widget->SetAlignmentInViewport(Alignment);

	// Base position is the anchor point + offset
	FVector2D FinalPos = AnchorPos + AnchorOffsetPx;

	// If placing relative to another widget, compute edge-to-edge placement.
	if (RelativeTo && Side != EWidgetSide::None)
	{
		const FVector2D OtherSize = RelativeTo->GetDesiredSize();

		// RelativeTo's anchor in viewport is not known here; so we assume RelativeTo is already positioned,
		// and we place next to it using its current viewport position.
		// (UMG doesn't expose a reliable "viewport rect" without geometry, so keep it simple.)
		// If you need true relative placement, pass RelativeTo's anchor/pos explicitly instead.

		// Fallback behavior: treat "RelativeTo" as centered at the same anchor.
		// (This keeps your helper deterministic.)
		switch (Side)
		{
		case EWidgetSide::Left:   FinalPos.X -= (OtherSize.X * (1.f - Alignment.X)) + (ThisSize.X * Alignment.X) + GapPx; break;
		case EWidgetSide::Right:  FinalPos.X += (OtherSize.X * Alignment.X)       + (ThisSize.X * (1.f - Alignment.X)) + GapPx; break;
		case EWidgetSide::Top:    FinalPos.Y -= (OtherSize.Y * (1.f - Alignment.Y)) + (ThisSize.Y * Alignment.Y) + GapPx; break;
		case EWidgetSide::Bottom: FinalPos.Y += (OtherSize.Y * Alignment.Y)       + (ThisSize.Y * (1.f - Alignment.Y)) + GapPx; break;
		default: break;
		}
	}

	Widget->SetPositionInViewport(FinalPos, bRemoveDPIScale);
	return true;
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
			SetDestinationClickAction, ETriggerEvent::Triggered, this, &AInvPlayerController::OnSetDestinationTriggered);
		EnhancedInputComponent->BindAction(
			SetDestinationClickAction, ETriggerEvent::Completed, this, &AInvPlayerController::OnSetDestinationCompleted);
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

bool AInvPlayerController::Player_SplitStack(int32 FromIndex, int32 ToIndex, int32 SplitAmount, TArray<int32>& OutChanged)
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
	const FVector TraceEnd   = Desired - FVector(0.f, 0.f, DropHeightTrace);

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

	FVector Loc; FRotator Rot;
	GetDropTransform(Loc, Rot);

	return InventoryComponent->DropFromSlot(SlotIndex, Quantity, Loc, Rot, OutChanged);
}

bool AInvPlayerController::Transfer_PlayerToExternal(int32 PlayerFromIndex, int32 ExternalToIndex, int32 Quantity,
                                                     TArray<int32>& OutChangedPlayer, TArray<int32>& OutChangedExternal)
{
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid() || !ExternalInventory.IsValid()) return false;

	// Source = player, target = external
	return InventoryComponent->TransferTo(ExternalInventory.Get(), PlayerFromIndex, ExternalToIndex, Quantity, OutChangedPlayer, OutChangedExternal);
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
                                                         TArray<int32>& OutChangedPlayer, TArray<int32>& OutChangedExternal)
{
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid() || !ExternalInventory.IsValid()) return false;

	return InventoryComponent->AutoTransferTo(ExternalInventory.Get(), PlayerFromIndex, Quantity, OutChangedPlayer, OutChangedExternal);
}

bool AInvPlayerController::AutoTransfer_ExternalToPlayer(int32 ExternalFromIndex, int32 Quantity,
                                                         TArray<int32>& OutChangedExternal, TArray<int32>& OutChangedPlayer)
{
	EnsurePlayerInventoryResolved();
	if (!InventoryComponent.IsValid() || !ExternalInventory.IsValid()) return false;

	return ExternalInventory->AutoTransferTo(ExternalInventory.Get(), ExternalFromIndex, Quantity, OutChangedExternal, OutChangedPlayer);
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

		PlaceWidgetInViewport(
			PlayerInventoryWidget,
			this,
			FVector2D(1.f, 0.5f),
			FVector2D(1.f, 0.5f),
			FVector2D(-40.f, 0.f)
		);
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

void AInvPlayerController::OpenExternalInventoryFromActor(AActor* ActorWithInventory)
{
}

void AInvPlayerController::CloseExternalInventory()
{
}
