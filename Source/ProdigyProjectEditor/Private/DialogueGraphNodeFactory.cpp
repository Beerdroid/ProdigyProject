#include "DialogueGraphNodeFactory.h"

#include "DialogueGraphNode.h"
#include "SDialogueGraphNode.h"

TSharedPtr<SGraphNode> FDialogueGraphNodeFactory::CreateNode(UEdGraphNode* Node) const
{
	if (UDialogueGraphNode* DialogueNode = Cast<UDialogueGraphNode>(Node))
	{
		return SNew(SDialogueGraphNode, DialogueNode);
	}
	return nullptr;
}
