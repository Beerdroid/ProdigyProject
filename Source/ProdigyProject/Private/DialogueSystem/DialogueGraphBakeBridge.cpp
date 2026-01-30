#include "DialogueSystem/DialogueGraphBakeBridge.h"

#if WITH_EDITOR

static FDialogueGraphBakeFn GDialogueGraphBakeFn = nullptr;

void RegisterDialogueGraphBakeFn(FDialogueGraphBakeFn InFn)
{
	GDialogueGraphBakeFn = InFn;
}

FDialogueGraphBakeFn GetDialogueGraphBakeFn()
{
	return GDialogueGraphBakeFn;
}

#endif // WITH_EDITOR
