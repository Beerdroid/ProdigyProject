#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "DialogueEdGraphSchema.generated.h"

UCLASS()
class PRODIGYPROJECTEDITOR_API UDialogueEdGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	// Custom pin category used by this schema
	static const FName PC_Dialogue;

	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;

	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;

	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;

	// Connection drawing style
	virtual FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const override;

	// Make this graph compatible with the editor
	virtual EGraphType GetGraphType(const UEdGraph* TestEdGraph) const override { return GT_StateMachine; }


private:
	static bool IsInputPin(const UEdGraphPin* Pin);
	static bool IsOutputPin(const UEdGraphPin* Pin);
};
