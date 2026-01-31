// Fill out your copyright notice in the Description page of Project Settings.


#include "DialogueSystem/DialogueDataAsset.h"

void UDialogueDataAsset::PostLoad()
{
	Super::PostLoad();
	RebuildNodeIndex();
}

#if WITH_EDITOR
void UDialogueDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RebuildNodeIndex();      // keep cache in sync while editing
	RebuildFlowPreview();    // your existing editor preview hook
}
#endif

void UDialogueDataAsset::RebuildNodeIndex()
{
	NodeIndexByID.Reset();
	NodeIndexByID.Reserve(Nodes.Num());

	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		const FName ID = Nodes[i].NodeID;
		if (ID.IsNone())
		{
			continue;
		}

		// First one wins; warn on duplicates
		if (NodeIndexByID.Contains(ID))
		{
			UE_LOG(LogTemp, Warning, TEXT("Dialogue '%s': Duplicate NodeID '%s' (keeping first)"),
				*GetNameSafe(this), *ID.ToString());
			continue;
		}

		NodeIndexByID.Add(ID, i);
	}
}

bool UDialogueDataAsset::TryGetNode(FName NodeID, FDialogueNode& OutNode) const
{
	if (NodeID.IsNone())
	{
		return false;
	}

	const int32* IndexPtr = NodeIndexByID.Find(NodeID);
	if (!IndexPtr)
	{
		// Fallback: in case cache wasn't built (edge cases), rebuild once and try again.
		// (Keeps behavior robust without changing external code.)
		UDialogueDataAsset* MutableThis = const_cast<UDialogueDataAsset*>(this);
		MutableThis->RebuildNodeIndex();

		IndexPtr = MutableThis->NodeIndexByID.Find(NodeID);
		if (!IndexPtr)
		{
			return false;
		}
	}

	const int32 Index = *IndexPtr;
	if (!Nodes.IsValidIndex(Index))
	{
		return false;
	}

	OutNode = Nodes[Index];
	return true;
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

#endif