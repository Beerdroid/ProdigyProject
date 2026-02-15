#pragma once

#include "NativeGameplayTags.h"

namespace GameItems
{
	namespace Equipment
	{
		namespace Weapons
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Axe)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sword)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Knife)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Staff)
		}

		namespace Chest
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heavy)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Medium)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Light)
		}


		namespace Cloak
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(RedCloak)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(BlueCloak)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(GreenCloak)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(YellowCloak)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(DarkGrayCloak)
		}

		namespace Head
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heavy)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Medium)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Light)
		}

		namespace Hands
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heavy)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Medium)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Light)
		}


		namespace Pants
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heavy)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Medium)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Light)
		}

		namespace Boots
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heavy)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Medium)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Light)
		}
	}

	namespace Consumables
	{
		namespace Potions
		{
			namespace Red
			{
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Small)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Large)
			}

			namespace Blue
			{
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Small)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Large)
			}
		}
	}

	namespace Craftables
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(FireFernFruit)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(LuminDaisy)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(ScorchPetalBlossom)
	}
}