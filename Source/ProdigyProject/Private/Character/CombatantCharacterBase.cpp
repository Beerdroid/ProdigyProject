
#include "Character/CombatantCharacterBase.h"

#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/StatusComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "AbilitySystem/ProdigyGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/HealthBarWidgetComponent.h"

ACombatantCharacterBase::ACombatantCharacterBase()
{
	Status       = CreateDefaultSubobject<UStatusComponent>(TEXT("Status"));
	ActionComponent = CreateDefaultSubobject<UActionComponent>(TEXT("ActionComponent"));
	Attributes = CreateDefaultSubobject<UAttributesComponent>(TEXT("Attributes"));
}

void ACombatantCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	WorldHealthBar = FindComponentByClass<UHealthBarWidgetComponent>();

	if (!IsValid(Attributes))
	{
		Attributes = FindComponentByClass<UAttributesComponent>();
	}

	if (IsValid(Attributes))
	{
		Attributes->OnAttributeChanged.RemoveDynamic(this, &ACombatantCharacterBase::HandleDeathIfNeeded);
		Attributes->OnAttributeChanged.AddDynamic(this, &ACombatantCharacterBase::HandleDeathIfNeeded);
	}
}

void ACombatantCharacterBase::EnableRagdoll()
{
	if (bDidRagdoll) return;
	bDidRagdoll = true;

	// Stop movement immediately
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
		Move->SetComponentTickEnabled(false);
	}

	// Disable capsule collision so ragdoll isn't fighting it
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetGenerateOverlapEvents(false);
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!IsValid(MeshComp)) return;

	// Make sure mesh can collide + simulate

	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetEnableGravity(true);
	MeshComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
}

void ACombatantCharacterBase::HandleDeathIfNeeded(FGameplayTag Tag, float NewValue, float Delta, AActor* InstigatorActor)
{
	if (bDidRagdoll) return;

	if (Tag != ProdigyTags::Attr::Health) return;

	if (NewValue > 0.f) return;

	RemoveHealthBar();
	EnableRagdoll();
}

void ACombatantCharacterBase::RemoveHealthBar()
{
	// Resolve lazily (works even if you forgot to cache)
	if (!IsValid(WorldHealthBar))
	{
		WorldHealthBar = FindComponentByClass<UHealthBarWidgetComponent>();
	}

	if (!IsValid(WorldHealthBar)) return;

	// Hard hide
	WorldHealthBar->SetVisibility(false, true);
	WorldHealthBar->SetHiddenInGame(true);

	// The reliable part: remove the actual widget content
	WorldHealthBar->SetWidget(nullptr);
	WorldHealthBar->SetWidgetClass(nullptr);

}

void ACombatantCharacterBase::OnCombatFreeze_Implementation(bool bFrozen)
{
	// Snap-freeze gameplay only; keep visuals alive.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (bFrozen)
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}
		else
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	// Pause AI decision-making (if AI controlled)
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (bFrozen)
		{
			AIC->StopMovement();
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("CombatFreeze"));
			}
		}
		else
		{
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->RestartLogic();
			}
		}
	}
}

// ---- IActionAgentInterface ----

void ACombatantCharacterBase::GetOwnedGameplayTags_Implementation(FGameplayTagContainer& OutTags) const
{
	OutTags.Reset();
	if (Status)
	{
		Status->GetOwnedTags(OutTags);
	}
}

bool ACombatantCharacterBase::AddStatusTag_Implementation(const FGameplayTag& StatusTag, int32 Turns, float Seconds, AActor* InstigatorActor)
{
	(void)InstigatorActor;
	return Status ? Status->AddStatusTag(StatusTag, Turns, Seconds) : false;
}

bool ACombatantCharacterBase::HasAttribute_Implementation(FGameplayTag AttributeTag) const
{
	return IsValid(Attributes) ? Attributes->HasAttribute(AttributeTag) : false;
}

float ACombatantCharacterBase::GetAttributeCurrentValue_Implementation(FGameplayTag AttributeTag) const
{
	return IsValid(Attributes) ? Attributes->GetCurrentValue(AttributeTag) : 0.f;
}

bool ACombatantCharacterBase::ModifyAttributeCurrentValue_Implementation(FGameplayTag AttributeTag, float Delta, AActor* InstigatorActor)
{
	return IsValid(Attributes) ? Attributes->ModifyCurrentValue(AttributeTag, Delta, InstigatorActor) : false;
}

float ACombatantCharacterBase::GetAttributeFinalValue_Implementation(FGameplayTag AttributeTag) const
{
	return IsValid(Attributes) ? Attributes->GetFinalValue(AttributeTag) : 0.f;
}
