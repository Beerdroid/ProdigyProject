#include "InvItemTags.h"

namespace GameItems
{
	namespace Equipment
	{
		namespace Weapons
		{
			UE_DEFINE_GAMEPLAY_TAG(Axe, "GameItems.Equipment.Weapons.Axe")
			UE_DEFINE_GAMEPLAY_TAG(Sword, "GameItems.Equipment.Weapons.Sword")
			UE_DEFINE_GAMEPLAY_TAG(Knife, "GameItems.Equipment.Weapons.Knife")
			UE_DEFINE_GAMEPLAY_TAG(Bloodthorn, "GameItems.Equipment.Weapons.Bloodthorn")
			UE_DEFINE_GAMEPLAY_TAG(NightsEdge, "GameItems.Equipment.Weapons.NightsEdge")
			UE_DEFINE_GAMEPLAY_TAG(Shadowbane, "GameItems.Equipment.Weapons.Shadowbane")
			UE_DEFINE_GAMEPLAY_TAG(Staff, "GameItems.Equipment.Weapons.Staff")
		}

		namespace Cloaks
		{
			UE_DEFINE_GAMEPLAY_TAG(RedCloak, "GameItems.Equipment.Cloaks.RedCloak")
			UE_DEFINE_GAMEPLAY_TAG(BlueCloak, "GameItems.Equipment.Cloaks.BlueCloak")
			UE_DEFINE_GAMEPLAY_TAG(GreenCloak, "GameItems.Equipment.Cloaks.GreenCloak")
			UE_DEFINE_GAMEPLAY_TAG(YellowCloak, "GameItems.Equipment.Cloaks.YellowCloak")
			UE_DEFINE_GAMEPLAY_TAG(DarkGrayCloak, "GameItems.Equipment.Cloaks.DarkGrayCloak")
		}

		namespace Chest
		{
			UE_DEFINE_GAMEPLAY_TAG(Heavy, "GameItems.Equipment.Chest.Heavy")
			UE_DEFINE_GAMEPLAY_TAG(Medium, "GameItems.Equipment.Chest.Medium")
			UE_DEFINE_GAMEPLAY_TAG(Light, "GameItems.Equipment.Chest.Light")
		}

		namespace Masks
		{
			UE_DEFINE_GAMEPLAY_TAG(SteelMask, "GameItems.Equipment.Masks.SteelMask")
			UE_DEFINE_GAMEPLAY_TAG(GreenMask, "GameItems.Equipment.Masks.SteelMask")
			UE_DEFINE_GAMEPLAY_TAG(BlueMask, "GameItems.Equipment.Masks.BlueMask")
			UE_DEFINE_GAMEPLAY_TAG(YellowMask, "GameItems.Equipment.Masks.YellowMask")
			UE_DEFINE_GAMEPLAY_TAG(GrayMask, "GameItems.Equipment.Masks.GrayMask")
		}
	}

	namespace Consumables
	{
		namespace Potions
		{
			namespace Red
			{
				UE_DEFINE_GAMEPLAY_TAG(Small, "GameItems.Consumables.Potions.Red.Small")
				UE_DEFINE_GAMEPLAY_TAG(Large, "GameItems.Consumables.Potions.Red.Large")
			}

			namespace Blue
			{
				UE_DEFINE_GAMEPLAY_TAG(Small, "GameItems.Consumables.Potions.Blue.Small")
				UE_DEFINE_GAMEPLAY_TAG(Large, "GameItems.Consumables.Potions.Blue.Large")
			}
		}
	}

	namespace Craftables
	{
		UE_DEFINE_GAMEPLAY_TAG(FireFernFruit, "GameItems.Craftables.FireFernFruit")
		UE_DEFINE_GAMEPLAY_TAG(LuminDaisy, "GameItems.Craftables.LuminDaisy")
		UE_DEFINE_GAMEPLAY_TAG(ScorchPetalBlossom, "GameItems.Craftables.ScorchPetalBlossom")
	}
}