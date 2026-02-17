#include "ProdigyInventory/InvItemTags.h"

namespace GameItems
{
	namespace Equipment
	{
		namespace Weapons
		{
			UE_DEFINE_GAMEPLAY_TAG(Axe, "GameItems.Equipment.Weapons.Axe")
			UE_DEFINE_GAMEPLAY_TAG(Sword, "GameItems.Equipment.Weapons.Sword")
			UE_DEFINE_GAMEPLAY_TAG(Knife, "GameItems.Equipment.Weapons.Knife")
			UE_DEFINE_GAMEPLAY_TAG(Staff, "GameItems.Equipment.Weapons.Staff")
		}

		namespace Cloak
		{
			UE_DEFINE_GAMEPLAY_TAG(RedCloak, "GameItems.Equipment.Cloak.RedCloak")
			UE_DEFINE_GAMEPLAY_TAG(BlueCloak, "GameItems.Equipment.Cloak.BlueCloak")
			UE_DEFINE_GAMEPLAY_TAG(GreenCloak, "GameItems.Equipment.Cloak.GreenCloak")
			UE_DEFINE_GAMEPLAY_TAG(YellowCloak, "GameItems.Equipment.Cloak.YellowCloak")
			UE_DEFINE_GAMEPLAY_TAG(DarkGrayCloak, "GameItems.Equipment.Cloak.DarkGrayCloak")
		}

		namespace Chest
		{
			UE_DEFINE_GAMEPLAY_TAG(Heavy, "GameItems.Equipment.Chest.Heavy")
			UE_DEFINE_GAMEPLAY_TAG(Medium, "GameItems.Equipment.Chest.Medium")
			UE_DEFINE_GAMEPLAY_TAG(Light, "GameItems.Equipment.Chest.Light")
		}

		namespace Pants
		{
			UE_DEFINE_GAMEPLAY_TAG(Heavy, "GameItems.Equipment.Pants.Heavy")
			UE_DEFINE_GAMEPLAY_TAG(Medium, "GameItems.Equipment.Pants.Medium")
			UE_DEFINE_GAMEPLAY_TAG(Light, "GameItems.Equipment.Pants.Light")
		}


		namespace Boots
		{
			UE_DEFINE_GAMEPLAY_TAG(Heavy, "GameItems.Equipment.Boots.Heavy")
			UE_DEFINE_GAMEPLAY_TAG(Medium, "GameItems.Equipment.Boots.Medium")
			UE_DEFINE_GAMEPLAY_TAG(Light, "GameItems.Equipment.Boots.Light")
		}


		namespace Head
		{
			UE_DEFINE_GAMEPLAY_TAG(Heavy, "GameItems.Equipment.Head.Heavy")
			UE_DEFINE_GAMEPLAY_TAG(Medium, "GameItems.Equipment.Head.Medium")
			UE_DEFINE_GAMEPLAY_TAG(Light, "GameItems.Equipment.Head.Light")

		}

		namespace Hands
		{
			UE_DEFINE_GAMEPLAY_TAG(Heavy, "GameItems.Equipment.Hands.Heavy")
			UE_DEFINE_GAMEPLAY_TAG(Medium, "GameItems.Equipment.Hands.Medium")
			UE_DEFINE_GAMEPLAY_TAG(Light, "GameItems.Equipment.Hands.Light")

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