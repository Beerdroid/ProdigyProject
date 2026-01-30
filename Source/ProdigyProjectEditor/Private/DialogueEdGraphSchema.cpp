// Fill out your copyright notice in the Description page of Project Settings.


#include "DialogueEdGraphSchema.h"

#include "ConnectionDrawingPolicy.h"
#include "DialogueGraphNode.h"

// Add the connection drawing policy class inside the cpp file
class FDialogueConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
public:
	FDialogueConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
		: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements)
	{
		// Set up appearance
		ArrowRadius = 8.0f;
		ArrowImage = FAppStyle::GetBrush("Graph.Arrow");
	}

	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override
	{
		Params.WireColor = FLinearColor::White;
		Params.WireThickness = 1.5f;
		Params.bDrawBubbles = false;
		Params.bUserFlag1 = false; // Not bidirectional
        
		// The correct parameter names for pins
		Params.AssociatedPin1 = OutputPin;
		Params.AssociatedPin2 = InputPin;
	}
};

// Add this implementation to your existing UDialogueEdGraphSchema class
FConnectionDrawingPolicy* UDialogueEdGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
	return new FDialogueConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}


#define LOCTEXT_NAMESPACE "DialogueEdGraphSchema"

const FName UDialogueEdGraphSchema::PC_Dialogue(TEXT("Dialogue"));
const FPinConnectionResponse UDialogueEdGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
    // Add debug logging
    UE_LOG(LogTemp, Warning, TEXT("CanCreateConnection: Pin A=%s (Category=%s, Direction=%d), Pin B=%s (Category=%s, Direction=%d)"), 
        A ? *A->PinName.ToString() : TEXT("NULL"), 
        A ? *A->PinType.PinCategory.ToString() : TEXT("NULL"),
        A ? (int32)A->Direction : -1,
        B ? *B->PinName.ToString() : TEXT("NULL"), 
        B ? *B->PinType.PinCategory.ToString() : TEXT("NULL"),
        B ? (int32)B->Direction : -1);

    // Basic validation
    if (!A || !B) 
    {
        UE_LOG(LogTemp, Warning, TEXT("Connection failed: Invalid pins"));
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("BadPins", "Invalid pins"));
    }
    
    if (A == B) 
    {
        UE_LOG(LogTemp, Warning, TEXT("Connection failed: Same pin"));
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SamePin", "Same pin"));
    }

    // Check pin categories
    UE_LOG(LogTemp, Warning, TEXT("Pin categories: A=%s, B=%s, Expected=%s"), 
        *A->PinType.PinCategory.ToString(), 
        *B->PinType.PinCategory.ToString(),
        *PC_Dialogue.ToString());

    // Only allow input to output connections
    if ((A->Direction == EGPD_Output && B->Direction == EGPD_Input) ||
        (A->Direction == EGPD_Input && B->Direction == EGPD_Output))
    {
        // Check if they're our Dialogue type
        if (A->PinType.PinCategory == PC_Dialogue && B->PinType.PinCategory == PC_Dialogue)
        {
            // Don't allow connection to self
            if (A->GetOwningNode() == B->GetOwningNode())
            {
                UE_LOG(LogTemp, Warning, TEXT("Connection failed: Same node"));
                return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SameNode", "Cannot connect to self"));
            }
            
            UE_LOG(LogTemp, Warning, TEXT("Connection allowed!"));
            return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("ConnectOK", "Connection allowed"));
        }
    }
    
    // Default case - disallow connection
    return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("IncompatibleConnection", "Incompatible connection"));
}

namespace DialogueSchema_Internal
{
	// Minimal action that spawns our node
	class FDialogueGraphAction_NewNode : public FEdGraphSchemaAction
	{
	public:
		FDialogueGraphAction_NewNode()
			: FEdGraphSchemaAction(
				LOCTEXT("DialogueCategory", "Dialogue"),
				LOCTEXT("DialogueAddNode", "Add Dialogue Node"),
				LOCTEXT("DialogueAddNodeTooltip", "Creates a dialogue node"),
				0)
		{
		}

		virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin,
			const FVector2D Location, bool bSelectNewNode = true) override
		{
			if (!ParentGraph) return nullptr;

			UDialogueGraphNode* NewNode = NewObject<UDialogueGraphNode>(ParentGraph);
			ParentGraph->Modify();
			ParentGraph->AddNode(NewNode, true, bSelectNewNode);

			NewNode->NodePosX = (int32)Location.X;
			NewNode->NodePosY = (int32)Location.Y;

			// Give a default choice so you can connect something immediately
			if (NewNode->Choices.Num() == 0)
			{
				NewNode->Choices.AddDefaulted();
			}

			NewNode->AllocateDefaultPins();
			NewNode->PostPlacedNewNode();
			NewNode->AutowireNewNode(FromPin);

			return NewNode;
		}
	};
}

bool UDialogueEdGraphSchema::IsInputPin(const UEdGraphPin* Pin)
{
	return Pin && Pin->Direction == EGPD_Input;
}

bool UDialogueEdGraphSchema::IsOutputPin(const UEdGraphPin* Pin)
{
	return Pin && Pin->Direction == EGPD_Output;
}

void UDialogueEdGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	// Right click on empty space -> allow adding node
	TSharedPtr<FEdGraphSchemaAction> NewAction = MakeShared<DialogueSchema_Internal::FDialogueGraphAction_NewNode>();
	ContextMenuBuilder.AddAction(NewAction);
}

void UDialogueEdGraphSchema::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	// Minimal: let UE default remove/break links etc.
	// You can add custom actions later (Add Choice, Remove Choice, etc.)
	Super::GetContextMenuActions(Menu, Context);
}

bool UDialogueEdGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	const FPinConnectionResponse Response = CanCreateConnection(A, B);
	if (Response.Response == CONNECT_RESPONSE_DISALLOW)
	{
		return false;
	}

	// Normalize A as output, B as input
	if (A->Direction == EGPD_Input)
	{
		Swap(A, B);
	}

	// Single-link policy: each choice output can have only one target
	if (A->LinkedTo.Num() > 0)
	{
		A->BreakAllPinLinks();
	}

	return UEdGraphSchema::TryCreateConnection(A, B);
}

void UDialogueEdGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	Super::BreakNodeLinks(TargetNode);
}

void UDialogueEdGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	Super::BreakPinLinks(TargetPin, bSendsNodeNotification);
}

#undef LOCTEXT_NAMESPACE
