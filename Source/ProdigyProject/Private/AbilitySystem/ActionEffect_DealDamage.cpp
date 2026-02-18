#include "AbilitySystem/ActionEffect_DealDamage.h"
#include "AbilitySystem/ActionAgentInterface.h"
#include "AbilitySystem/ActionComponent.h"
#include "AbilitySystem/ActionCueSettings.h"
#include "AbilitySystem/ActionCueSubsystem.h"
#include "AbilitySystem/ProdigyGameplayTags.h"

class UActionCueSettings;

bool UActionEffect_DealDamage::Apply_Implementation(const FActionContext& Context) const
{
	if (!IsValid(Context.TargetActor)) return false;
	if (!IsValid(Context.Instigator)) return false;

	const FGameplayTag HealthTag = HealthAttributeTag.IsValid()
		? HealthAttributeTag
		: ProdigyTags::Attr::Health;

	if (!Context.TargetActor->GetClass()->ImplementsInterface(UActionAgentInterface::StaticClass()))
	{
		UE_LOG(LogActionExec, Error,
			TEXT("[DealDamage] Target %s missing ActionAgentInterface"),
			*GetNameSafe(Context.TargetActor));
		return false;
	}

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

	const float AppliedDamage = FMath::Max(0.f, Damage);
	const float Delta = -AppliedDamage;

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
			const float ClampDelta = 0.f - NewHP;
			(void)IActionAgentInterface::Execute_ModifyAttributeCurrentValue(
				Context.TargetActor, HealthTag, ClampDelta, Context.Instigator);
		}
	}

	const float FinalHP = IActionAgentInterface::Execute_GetAttributeCurrentValue(Context.TargetActor, HealthTag);

	// --- CUE (with pre/post HP snapshots so killing blow hit plays) ---
	if (bPlayHitCue && AppliedDamage > 0.f)
	{
		UWorld* World = nullptr;
		if (IsValid(Context.Instigator)) World = Context.Instigator->GetWorld();
		if (!World && IsValid(Context.TargetActor)) World = Context.TargetActor->GetWorld();

		if (World)
		{
			if (UActionCueSubsystem* Cues = World->GetSubsystem<UActionCueSubsystem>())
			{
				FGameplayTag SurfaceTag;
				FVector CueLoc = Context.TargetActor->GetActorLocation();

				if (const UActionCueSettings* Settings = GetDefault<UActionCueSettings>())
				{
					SurfaceTag = Settings->DefaultSurfaceTag;
				}

				if (bResolveSurfaceTypeByTrace)
				{
					const FVector From = Context.Instigator->GetActorLocation();

					FVector To = Context.TargetActor->GetActorLocation();
					To += (To - From).GetSafeNormal() * FMath::Max(0.f, SurfaceTraceDistanceExtra);

					FHitResult Hit;
					FCollisionQueryParams Params(SCENE_QUERY_STAT(ActionCueSurfaceTrace), false);
					Params.AddIgnoredActor(Context.Instigator);
					Params.bReturnPhysicalMaterial = true;

					const bool bHit = World->LineTraceSingleByChannel(Hit, From, To, SurfaceTraceChannel, Params);
					if (bHit)
					{
						CueLoc = Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint;

						if (Hit.PhysMaterial.IsValid())
						{
							if (const UActionCueSettings* Settings = GetDefault<UActionCueSettings>())
							{
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

				FActionCueContext CueCtx;
				CueCtx.InstigatorActor = Context.Instigator;
				CueCtx.TargetActor = Context.TargetActor;
				CueCtx.Location = CueLoc;
				CueCtx.OptionalSubTarget = Context.OptionalSubTarget;
				CueCtx.WeaponTag = WeaponTagForCue;
				CueCtx.SurfaceTag = SurfaceTag;

				// snapshots for gating
				CueCtx.TargetHPBefore = OldHP;
				CueCtx.TargetHPAfter = FinalHP;
				CueCtx.AppliedDamage = AppliedDamage;

				// Use the subsystem directly so it receives the extended context
				const FGameplayTag HitCueTag = FGameplayTag::RequestGameplayTag(FName("Cue.Action.Hit"));
				Cues->PlayCue(HitCueTag, CueCtx);
			}
		}
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