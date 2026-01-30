#pragma once

#include "CoreMinimal.h"

class UDialogueDataAsset;

#if WITH_EDITOR

// Plain C-style function pointer (export-safe across modules/DLLs)
using FDialogueGraphBakeFn = void(*)(UDialogueDataAsset*);

PRODIGYPROJECT_API void RegisterDialogueGraphBakeFn(FDialogueGraphBakeFn InFn);
PRODIGYPROJECT_API FDialogueGraphBakeFn GetDialogueGraphBakeFn();

#endif // WITH_EDITOR
