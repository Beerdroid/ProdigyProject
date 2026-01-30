// DialogueDataAsset.cpp  (Runtime module: ProdigyProject)

#include "DialogueSystem/DialogueDataAsset.h"
#include "DialogueSystem/DialogueGraphBakeBridge.h"

bool UDialogueDataAsset::TryGetNode(FName NodeID, FDialogueNode& OutNode) const
{
	if (NodeID.IsNone())
	{
		return false;
	}

	if (const FDialogueNode* Found = RuntimeNodes.Find(NodeID))
	{
		OutNode = *Found;
		return true;
	}

	return false;
}

void UDialogueDataAsset::Editor_Compile()
{
#if WITH_EDITOR
	Modify();
	RebuildRuntimeData();
	MarkPackageDirty();
#endif
}

#if WITH_EDITOR

void UDialogueDataAsset::RebuildRuntimeData()
{
	RuntimeNodes.Reset();

	if (AuthoringMode == EDialogueAuthoringMode::ManualNodes)
	{
		RebuildRuntimeDataFromManualNodes();
	}
	else
	{
		// Implemented in ProdigyProjectEditor module (.cpp) to avoid runtime->editor dependency.
		RebuildRuntimeDataFromGraph();
	}
}

void UDialogueDataAsset::RebuildRuntimeDataFromManualNodes()
{
	for (const FDialogueNode& N : Nodes)
	{
		if (N.NodeID.IsNone())
		{
			continue;
		}

		// "Last one wins" if duplicated IDs exist (you can add logging if desired).
		RuntimeNodes.Add(N.NodeID, N);
	}
}

void UDialogueDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RebuildRuntimeData();
	MarkPackageDirty();
}

void UDialogueDataAsset::PostLoad()
{
	Super::PostLoad();

	// Keep RuntimeNodes consistent when loading in editor.
	RebuildRuntimeData();
}

void UDialogueDataAsset::RebuildRuntimeDataFromGraph()
{
	// Runtime module doesn't know graph node types. It just calls the editor-registered function.
	if (FDialogueGraphBakeFn Fn = GetDialogueGraphBakeFn())
	{
		Fn(this);
	}
}

#endif // WITH_EDITOR
