#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"

class UDialogueGraphNode;

class SDialogueGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SDialogueGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UDialogueGraphNode* InNode);

	// SGraphNode
	virtual void UpdateGraphNode() override;

protected:
	// Helpers
	TSharedRef<SWidget> CreateTitleWidget();

	virtual void CreatePinWidgets() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
};
