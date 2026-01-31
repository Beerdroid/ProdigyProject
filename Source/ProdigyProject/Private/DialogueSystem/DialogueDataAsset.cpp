// Fill out your copyright notice in the Description page of Project Settings.


#include "DialogueSystem/DialogueDataAsset.h"

bool UDialogueDataAsset::TryGetNode(FName NodeID, FDialogueNode& OutNode) const
{
	if (NodeID.IsNone()) return false;

	for (const FDialogueNode& N : Nodes)
	{
		if (N.NodeID == NodeID)
		{
			OutNode = N;
			return true;
		}
	}
	return false;
}

#if WITH_EDITOR

void UDialogueDataAsset::RebuildFlowPreview()
{
	TArray<FString> Lines;

	// Your EXISTING logic – unchanged
	for (const FDialogueNode& N : Nodes)
	{
		if (N.NodeID.IsNone()) continue;

		for (const FDialogueChoice& C : N.Choices)
		{
			if (!C.NextNodeID.IsNone())
			{
				Lines.Add(FString::Printf(
					TEXT("%s -> %s"),
					*N.NodeID.ToString(),
					*C.NextNodeID.ToString()));
			}
			else
			{
				Lines.Add(FString::Printf(
					TEXT("%s -> <END>"),
					*N.NodeID.ToString()));
			}
		}
	}

	FlowPreview = FString::Join(Lines, TEXT("\n"));
}

void UDialogueDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildFlowPreview();
}
#endif