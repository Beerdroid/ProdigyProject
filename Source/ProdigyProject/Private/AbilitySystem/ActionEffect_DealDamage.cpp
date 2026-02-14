#include "AbilitySystem/ActionEffect_DealDamage.h"
#include "AbilitySystem/ActionAgentInterface.h"
#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/ActionCueLibrary.h"
#include "AbilitySystem/ActionCueSettings.h"
#include "AbilitySystem/ProdigyGameplayTags.h"

class UActionCueSettings;

bool UActionEffect_DealDamage::Apply_Implementation(const FActionContext& Context) const
{
	if (!IsValid(Context.TargetActor)) return false;
	if (!IsValid(Context.Instigator)) return false;

	// Health tag (explicit override if provided)
	const FGameplayTag HealthTag = HealthAttributeTag.IsValid()
		? HealthAttributeTag
		: ProdigyTags::Attr::Health;

	// Must implement ActionAgentInterface
	if (!Context.TargetActor->GetClass()->ImplementsInterface(UActionAgentInterface::StaticClass()))
	{
		UE_LOG(LogActionExec, Error,
			TEXT("[DealDamage] Target %s missing ActionAgentInterface"),
			*GetNameSafe(Context.TargetActor));
		return false;
	}

	// Must have health attribute
	const bool bHasHealth = IActionAgentInterface::Execute_HasAttribute(Context.TargetActor, HealthTag);
	if (!bHasHealth)
	{
		UE_LOG(LogActionExec, Error,
			TEXT("[DealDamage] Target %s missing attribute %s in DefaultAttributes"),
			*GetNameSafe(Context.TargetActor),
			*HealthTag.ToString());
		return false;
	}

	const float OldHP = IActionAgentInterface::Execute_GetAttributeCurrentValue(Context.TargetActor, HealthTag);

	// Apply damage as negative delta (ignore negative Damage values)
	const float AppliedDamage = FMath::Max(0.f, Damage);
	const float Delta = -AppliedDamage;

	// No-op damage should still succeed (and should not try to play cues)
	if (FMath::IsNearlyZero(Delta))
	{
		return true;
	}

	const bool bOk = IActionAgentInterface::Execute_ModifyAttributeCurrentValue(
		Context.TargetActor, HealthTag, Delta, Context.Instigator);
	if (!bOk) return false;

	if (bClampMinZero)
	{
		const float NewHP = IActionAgentInterface::Execute_GetAttributeCurrentValue(Context.TargetActor, HealthTag);
		if (NewHP < 0.f)
		{
			// Clamp explicitly via delta to keep broadcast consistent
			const float ClampDelta = 0.f - NewHP;
			(void)IActionAgentInterface::Execute_ModifyAttributeCurrentValue(
				Context.TargetActor, HealthTag, ClampDelta, Context.Instigator);
		}
	}

	const float FinalHP = IActionAgentInterface::Execute_GetAttributeCurrentValue(Context.TargetActor, HealthTag);

	if (bPlayHitCue && AppliedDamage > 0.f)
	{
		UObject* WorldContextObject = Context.Instigator ? (UObject*)Context.Instigator : (UObject*)Context.TargetActor;

		FGameplayTag SurfaceTag;
		FVector CueLoc = Context.TargetActor->GetActorLocation();

		// Default surface tag from settings (if configured)
		if (const UActionCueSettings* Settings = GetDefault<UActionCueSettings>())
		{
			SurfaceTag = Settings->DefaultSurfaceTag;
		}

		if (bResolveSurfaceTypeByTrace)
		{
			UWorld* World = Context.Instigator->GetWorld();
			if (World)
			{
				const FVector From = Context.Instigator->GetActorLocation();

				FVector To = Context.TargetActor->GetActorLocation();
				To += (To - From).GetSafeNormal() * FMath::Max(0.f, SurfaceTraceDistanceExtra);

				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(ActionCueSurfaceTrace), /*bTraceComplex*/ false);
				Params.AddIgnoredActor(Context.Instigator);
				Params.bReturnPhysicalMaterial = true;

				const bool bHit = World->LineTraceSingleByChannel(Hit, From, To, SurfaceTraceChannel, Params);
				if (bHit)
				{
					CueLoc = Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint;

					if (Hit.PhysMaterial.IsValid())
					{
						// Resolve custom surface tag from physical material via settings mapping
						if (const UActionCueSettings* Settings = GetDefault<UActionCueSettings>())
						{
							// Default if no mapping matches
							FGameplayTag Resolved = Settings->DefaultSurfaceTag;

							for (const FActionCueSurfaceTagMapEntry& E : Settings->SurfaceTagsByPhysicalMaterial)
							{
								if (E.PhysicalMaterial.IsNull()) continue;

								const UPhysicalMaterial* PM = E.PhysicalMaterial.LoadSynchronous();
								if (PM && PM == Hit.PhysMaterial.Get())
								{
									if (E.SurfaceTag.IsValid())
									{
										Resolved = E.SurfaceTag;
									}
									break;
								}
							}

							SurfaceTag = Resolved;
						}
					}
				}
			}
		}

		UActionCueLibrary::PlayHitCue_Layered(
			WorldContextObject,
			Context.TargetActor,
			Context.Instigator,
			AppliedDamage,
			/*OptionalWorldLocation*/ CueLoc,
			/*WeaponTag*/ WeaponTagForCue,
			/*SurfaceTag*/ SurfaceTag
		);
	}

	UE_LOG(LogActionExec, Warning,
		TEXT("[DealDamage] %s HP %.2f -> %.2f (Damage=%.2f) Inst=%s"),
		*GetNameSafe(Context.TargetActor),
		OldHP,
		FinalHP,
		AppliedDamage,
		*GetNameSafe(Context.Instigator));

	return true;
}
