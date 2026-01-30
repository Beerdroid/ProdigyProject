// DialogueDataAsset_GraphBake.cpp  (Editor module: ProdigyProjectEditor)

#include "DialogueDataAsset_GraphBake.h"

#if WITH_EDITOR

#include "DialogueGraphNode.h"
#include "DialogueSystem/DialogueDataAsset.h"        // runtime asset
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

static UDialogueGraphNode* CastDialogueNode(UEdGraphNode* Node)
{
	return Cast<UDialogueGraphNode>(Node);
}

static FName GetLinkedNodeId(UEdGraphPin* FromPin)
{
	if (!FromPin || FromPin->LinkedTo.Num() == 0) return NAME_None;

	UEdGraphPin* Linked = FromPin->LinkedTo[0];
	if (!Linked) return NAME_None;

	if (UDialogueGraphNode* Target = Cast<UDialogueGraphNode>(Linked->GetOwningNode()))
	{
		return Target->NodeID;
	}
	return NAME_None;
}

void FDialogueGraphBakerEditor::Bake(UDialogueDataAsset* Asset)
{
	if (!Asset) return;

	Asset->RuntimeNodes.Reset();

#if WITH_EDITORONLY_DATA
	UEdGraph* Graph = Asset->EditorGraph;
	if (!Graph) return;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UDialogueGraphNode* GN = Cast<UDialogueGraphNode>(Node);
		if (!GN) continue;
		if (GN->NodeID.IsNone()) continue;

		FDialogueNode Out;
		Out.NodeID    = GN->NodeID;
		Out.SpeakerID = GN->SpeakerID;
		Out.Text      = GN->Text;
		Out.Choices   = GN->Choices;

		for (int32 i = 0; i < Out.Choices.Num(); ++i)
		{
			UEdGraphPin* ChoicePin = GN->GetChoicePin(i);
			Out.Choices[i].NextNodeID = GetLinkedNodeId(ChoicePin);
		}

		Asset->RuntimeNodes.Add(Out.NodeID, MoveTemp(Out));
	}
#endif
}

#endif // WITH_EDITOR
