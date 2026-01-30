// DialogueDataAsset_GraphBake.h  (Editor module: ProdigyProjectEditor)
// Purpose: compile/bake UDialogueDataAsset from its EditorGraph into RuntimeNodes.
// This file lives in the Editor module so it can include UDialogueGraphNode.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

class UDialogueDataAsset;

/**
 * Editor-only helper that bakes graph authordata (UEdGraph + UDialogueGraphNode)
 * into runtime dialogue data (RuntimeNodes map) on the asset.
 *
 * NOTE: We keep this as a helper so the asset stays in the runtime module and
 * does not include editor-only headers.
 */
struct FDialogueGraphBakerEditor
{
	static void Bake(UDialogueDataAsset* Asset);
};

#endif // WITH_EDITOR