#include "AbilitySystem/ProdigyGameplayTags.h"

// =======================
// ACTION TAGS
// =======================
namespace ProdigyTags::Action::Attack
{
	UE_DEFINE_GAMEPLAY_TAG(Basic,   "Action.Attack.Basic");
	UE_DEFINE_GAMEPLAY_TAG(Power,   "Action.Attack.Power");
	UE_DEFINE_GAMEPLAY_TAG(Ranged,  "Action.Attack.Ranged");
}

namespace ProdigyTags::Action::Utility
{
	UE_DEFINE_GAMEPLAY_TAG(Stun,  "Action.Utility.Stun");
	UE_DEFINE_GAMEPLAY_TAG(Throw, "Action.Utility.Throw");
}

namespace ProdigyTags::Action::Sneak
{
	UE_DEFINE_GAMEPLAY_TAG(Takedown,   "Action.Sneak.Takedown");
	UE_DEFINE_GAMEPLAY_TAG(Pickpocket, "Action.Sneak.Pickpocket");
}

namespace ProdigyTags::Action::Social
{
	UE_DEFINE_GAMEPLAY_TAG(Intimidate, "Action.Social.Intimidate");
	UE_DEFINE_GAMEPLAY_TAG(Persuade,   "Action.Social.Persuade");
}

namespace ProdigyTags::Action::Heal
{
	UE_DEFINE_GAMEPLAY_TAG(Self, "Action.Heal.Self");
	UE_DEFINE_GAMEPLAY_TAG(Target,   "Action.Heal.Target");
}

// =======================
// STATUS TAGS
// =======================
namespace ProdigyTags::Status
{
	UE_DEFINE_GAMEPLAY_TAG(Stunned,   "Status.Stunned");
	UE_DEFINE_GAMEPLAY_TAG(Stealthed, "Status.Stealthed");
	UE_DEFINE_GAMEPLAY_TAG(Surprised, "Status.Surprised");
	UE_DEFINE_GAMEPLAY_TAG(Bleeding,  "Status.Bleeding");
}

// =======================
// MODE / STATE TAGS
// =======================
namespace ProdigyTags::Mode
{
	UE_DEFINE_GAMEPLAY_TAG(Combat,      "Mode.Combat");
	UE_DEFINE_GAMEPLAY_TAG(Exploration, "Mode.Exploration");
}

namespace ProdigyTags::State
{
	UE_DEFINE_GAMEPLAY_TAG(CanAct,        "State.CanAct");
	UE_DEFINE_GAMEPLAY_TAG(CombatLocked,  "State.CombatLocked");
	UE_DEFINE_GAMEPLAY_TAG(InDialogue,    "State.InDialogue");
}

// =======================
// ATTRIBUTE TAGS
// =======================
namespace ProdigyTags::Attr
{
	UE_DEFINE_GAMEPLAY_TAG(Health,    "Attr.Health");
	UE_DEFINE_GAMEPLAY_TAG(MaxHealth, "Attr.MaxHealth");

	UE_DEFINE_GAMEPLAY_TAG(AP,     "Attr.AP");
	UE_DEFINE_GAMEPLAY_TAG(MaxAP,  "Attr.MaxAP");
}



// =======================
// CUE TAGS
// =======================
UE_DEFINE_GAMEPLAY_TAG(ActionCueTags::Cue_Combat_Enter,   "Cue.Combat.Enter");
UE_DEFINE_GAMEPLAY_TAG(ActionCueTags::Cue_Combat_Exit,    "Cue.Combat.Exit");

UE_DEFINE_GAMEPLAY_TAG(ActionCueTags::Cue_Target_Lock,    "Cue.Target.Lock");
UE_DEFINE_GAMEPLAY_TAG(ActionCueTags::Cue_Target_Invalid, "Cue.Target.Invalid");

UE_DEFINE_GAMEPLAY_TAG(ActionCueTags::Cue_Action_Hit,     "Cue.Action.Hit");
UE_DEFINE_GAMEPLAY_TAG(ActionCueTags::Cue_Action_Miss,    "Cue.Action.Miss");

// =======================
// EFFECT TAGS
// =======================
namespace ProdigyTags::Effect
{
	UE_DEFINE_GAMEPLAY_TAG(Burning,    "Effect.Burning");
	UE_DEFINE_GAMEPLAY_TAG(Regeneration, "Effect.Regeneration");

	UE_DEFINE_GAMEPLAY_TAG(Poison,     "Effect.Poison");
	UE_DEFINE_GAMEPLAY_TAG(Bleed,  "Effect.Bleed");
}
