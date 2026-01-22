// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/Inv_PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Interaction/Inv_Highlightable.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/HUD/Inv_HUDWidget.h"

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

	// TraceForItem();
}

void AInv_PlayerController::ToggleInventory()
{
	if (!InventoryComponent.IsValid()) return;
	InventoryComponent->ToggleInventoryMenu();

	if (InventoryComponent->IsMenuOpen())
	{
		HUDWidget->SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		HUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void AInv_PlayerController::BeginPlay()
{
	Super::BeginPlay();

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (IsValid(Subsystem))
	{
		Subsystem->AddMappingContext(DefaultIMC, 0);
	}

	InventoryComponent = FindComponentByClass<UInv_InventoryComponent>();
	CreateHUDWidget();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	GetWorldTimerManager().SetTimer(TraceTimerHandle, this, &AInv_PlayerController::TraceUnderMouseForItem, 0.05f, true);
}

void AInv_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);

	EnhancedInputComponent->BindAction(PrimaryInteractAction, ETriggerEvent::Started, this, &AInv_PlayerController::PrimaryInteract);
	EnhancedInputComponent->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &AInv_PlayerController::ToggleInventory);
}

void AInv_PlayerController::PrimaryInteract()
{
	if (!ThisActor.IsValid()) return;

	UInv_ItemComponent* ItemComp = ThisActor->FindComponentByClass<UInv_ItemComponent>();
	if (!IsValid(ItemComp) || !InventoryComponent.IsValid()) return;

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
		if (UActorComponent* Highlightable = ThisActor->FindComponentByInterface(UInv_Highlightable::StaticClass()); IsValid(Highlightable))
		{
			IInv_Highlightable::Execute_Highlight(Highlightable);
		}
		
		UInv_ItemComponent* ItemComponent = ThisActor->FindComponentByClass<UInv_ItemComponent>();
		if (!IsValid(ItemComponent)) return;

		if (IsValid(HUDWidget)) HUDWidget->ShowPickupMessage(ItemComponent->GetPickupMessage());
	}

	if (LastActor.IsValid())
	{
		if (UActorComponent* Highlightable = LastActor->FindComponentByInterface(UInv_Highlightable::StaticClass()); IsValid(Highlightable))
		{
			IInv_Highlightable::Execute_UnHighlight(Highlightable);
		}
	}
}

void AInv_PlayerController::TraceUnderMouseForItem()
{
    if (!IsLocalController())
    {
        UE_LOG(LogInvTrace, Verbose, TEXT("TraceForItem: Not local controller"));
        return;
    }

    FHitResult HitResult;

    const bool bHit = GetHitResultUnderCursorByChannel(
        UEngineTypes::ConvertToTraceType(ItemTraceChannel),
        /*bTraceComplex*/ false,
        HitResult
    );

    // --- DEBUG: trace result ---
    if (bHit)
    {
        UE_LOG(LogInvTrace, Verbose,
            TEXT("TraceForItem: HIT actor=%s location=%s"),
            *GetNameSafe(HitResult.GetActor()),
            *HitResult.ImpactPoint.ToString()
        );
    }
    else
    {
        UE_LOG(LogInvTrace, Verbose, TEXT("TraceForItem: NO HIT"));
    }

    LastActor = ThisActor;
    ThisActor = bHit ? HitResult.GetActor() : nullptr;

    // --- DEBUG: actor transition ---
    if (ThisActor != LastActor)
    {
        UE_LOG(LogInvTrace, Log,
            TEXT("TraceForItem: Actor changed: %s -> %s"),
            *GetNameSafe(LastActor.Get()),
            *GetNameSafe(ThisActor.Get())
        );

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1,
                1.0f,
                FColor::Cyan,
                FString::Printf(
                    TEXT("Trace Actor: %s"),
                    *GetNameSafe(ThisActor.Get())
                )
            );
        }
    }

    // No hit → clear HUD
    if (!ThisActor.IsValid())
    {
        UE_LOG(LogInvTrace, Verbose, TEXT("TraceForItem: Clearing pickup message"));

        if (IsValid(HUDWidget))
        {
            HUDWidget->HidePickupMessage();
        }
    }

    // If nothing changed, stop here
    if (ThisActor == LastActor)
    {
        UE_LOG(LogInvTrace, VeryVerbose, TEXT("TraceForItem: Same actor, skipping"));
        return;
    }

    // --- Unhighlight previous actor ---
    if (LastActor.IsValid())
    {
        if (UActorComponent* Highlightable =
                LastActor->FindComponentByInterface(UInv_Highlightable::StaticClass());
            IsValid(Highlightable))
        {
            UE_LOG(LogInvTrace, Log,
                TEXT("TraceForItem: UnHighlight %s"),
                *GetNameSafe(LastActor.Get())
            );

            IInv_Highlightable::Execute_UnHighlight(Highlightable);
        }
    }

    // --- Highlight new actor + show message ---
    if (ThisActor.IsValid())
    {
        if (UActorComponent* Highlightable =
                ThisActor->FindComponentByInterface(UInv_Highlightable::StaticClass());
            IsValid(Highlightable))
        {
            UE_LOG(LogInvTrace, Log,
                TEXT("TraceForItem: Highlight %s"),
                *GetNameSafe(ThisActor.Get())
            );

            IInv_Highlightable::Execute_Highlight(Highlightable);
        }

        UInv_ItemComponent* ItemComponent =
            ThisActor->FindComponentByClass<UInv_ItemComponent>();

        if (IsValid(ItemComponent))
        {
            UE_LOG(LogInvTrace, Log,
                TEXT("TraceForItem: ItemComponent found on %s"),
                *GetNameSafe(ThisActor.Get())
            );

            if (IsValid(HUDWidget))
            {
                HUDWidget->ShowPickupMessage(ItemComponent->GetPickupMessage());
            }
        }
        else
        {
            UE_LOG(LogInvTrace, Verbose,
                TEXT("TraceForItem: No ItemComponent on %s"),
                *GetNameSafe(ThisActor.Get())
            );

            if (IsValid(HUDWidget))
            {
                HUDWidget->HidePickupMessage();
            }
        }
    }
}
