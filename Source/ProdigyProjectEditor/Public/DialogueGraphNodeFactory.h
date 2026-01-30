#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"

class FDialogueGraphNodeFactory : public FGraphPanelNodeFactory
{
public:
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override;
};
