#pragma once

#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

// =======================
// ACTION TAGS
// =======================
namespace ProdigyTags::Action
{
	namespace Attack
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Basic);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Power);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ranged);
	}

	namespace Utility
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stun);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Throw);
	}

	namespace Sneak
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Takedown);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pickpocket);
	}

	namespace Social
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Intimidate);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Persuade);
	}
}

// =======================
// STATUS TAGS
// =======================
namespace ProdigyTags::Status
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stunned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stealthed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Surprised);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bleeding);
}

// =======================
// MODE / STATE TAGS
// =======================
namespace ProdigyTags::Mode
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Exploration);
}

namespace ProdigyTags::State
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(CanAct);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(CombatLocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InDialogue);
}

namespace ProdigyTags::Attr
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Health);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxHealth);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AP);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(MaxAP);
}

namespace ActionCueTags
{
	// Combat
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Combat_Enter);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Combat_Exit);

	// Targeting
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Target_Lock);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Target_Invalid);

	// Actions
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Action_Hit);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Action_Miss);

}