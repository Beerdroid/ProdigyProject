#include "SDialogueGraphNode.h"

#include "DialogueGraphNode.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"

void SDialogueGraphNode::Construct(const FArguments& InArgs, UDialogueGraphNode* InNode)
{
	this->GraphNode = InNode;
	this->SetCursor(EMouseCursor::CardinalCross);
	UpdateGraphNode();
}

TSharedRef<SWidget> SDialogueGraphNode::CreateTitleWidget()
{
	UDialogueGraphNode* N = CastChecked<UDialogueGraphNode>(GraphNode);

	return SNew(STextBlock)
		.Text(N->GetNodeTitle(ENodeTitleType::FullTitle))
		.WrapTextAt(300.f);
}

void SDialogueGraphNode::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	// Main layout
	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

	this->GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.Padding(6)
		[
			SNew(SVerticalBox)

			// Title
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreateTitleWidget()
			]

			// Pins area
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 6)
			[
				SNew(SHorizontalBox)

				// Inputs
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(LeftNodeBox, SVerticalBox)
				]

				// Spacer
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SSpacer)
				]

				// Outputs
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			]
		]
	];

	CreatePinWidgets();
}
