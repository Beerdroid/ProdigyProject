#include "InvContextMenuWidget.h"

#include "Components/SpinBox.h"
#include "ProdigyInventory/InventoryComponent.h"
#include "ProdigyInventory/InvPlayerController.h"
#include "InventoryWidgetBase.h" // your menu class
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogInvCtx, Log, All);

#define INV_CTX_LOG(Verbosity, Fmt, ...) \
UE_LOG(LogInvCtx, Verbosity, TEXT("[CTX:%s] " Fmt), *GetNameSafe(this), ##__VA_ARGS__)

static int32 ClampSplitAmount(int32 Amount, int32 Max)
{
	return FMath::Clamp(Amount, 1, FMath::Max(1, Max));
}

void UInvContextMenuWidget::Init(AInvPlayerController* InPC, UInventoryComponent* InInv, int32 InSlotIndex, bool bInExternal, UInventoryWidgetBase* InOwnerMenu)
{
	PC = InPC;
	Inventory = InInv;
	SlotIndex = InSlotIndex;
	bExternal = bInExternal;
	OwnerMenu = InOwnerMenu;

	const FInventorySlot S = IsValid(Inventory) ? Inventory->GetSlot(SlotIndex) : FInventorySlot();
	const bool bHasItem = !S.IsEmpty();

	// Max split is Qty-1 (can't split all)
	SplitMax = bHasItem ? FMath::Max(1, S.Quantity - 1) : 1;
	SplitAmount = ClampSplitAmount(SplitMax / 2, SplitMax); // default "half"

	INV_CTX_LOG(Warning,
		TEXT("Init: PC=%s Inv=%s Slot=%d HasItem=%d Qty=%d SplitMax=%d SplitAmount=%d OwnerMenu=%s External=%d"),
		*GetNameSafe(PC),
		*GetNameSafe(Inventory),
		SlotIndex,
		bHasItem ? 1 : 0,
		S.Quantity,
		SplitMax,
		SplitAmount,
		*GetNameSafe(OwnerMenu),
		bExternal ? 1 : 0);

	// UI values ONLY (no AddDynamic here)
	if (IsValid(SplitSlider))
	{
		SplitSlider->SetValue(SplitMax > 1 ? (float)SplitAmount / (float)SplitMax : 1.f);
		SplitSlider->SetIsEnabled(SplitMax > 1);
	}

	RefreshSplitAmountText();

	// BP visibility rules
	bool bStackable = false;
	if (bHasItem)
	{
		FItemRow Row;
		if (Inventory->TryGetItemDef(S.ItemID, Row))
		{
			bStackable = Row.bStackable;
		}
	}
	ConfigureForSlot(bHasItem, bStackable, S.Quantity);
}

void UInvContextMenuWidget::Action_ConsumeOne()
{
	INV_CTX_LOG(Warning, TEXT("Consume 1"));
	if (!IsValid(PC) || !IsValid(Inventory)) return;

	INV_CTX_LOG(Warning, TEXT("Consume"));

	// Only player inventory consumes (optional policy)
	TArray<int32> Changed;
	PC->Player_ConsumeOne(SlotIndex, Changed);
	Close();
}

void UInvContextMenuWidget::Action_DropOne()
{
	INV_CTX_LOG(Warning, TEXT("Drop1"));
	if (!IsValid(PC) || !IsValid(Inventory)) return;

	INV_CTX_LOG(Warning, TEXT("Drop"));
	
	TArray<int32> Changed;
	PC->Player_DropFromSlot(SlotIndex, 1, Changed);
	Close();
}

void UInvContextMenuWidget::Action_DropAll()
{
	INV_CTX_LOG(Warning, TEXT("DropAll 1"));
	
	if (!IsValid(PC) || !IsValid(Inventory)) return;

	INV_CTX_LOG(Warning, TEXT("DropAll"));

	const FInventorySlot S = Inventory->GetSlot(SlotIndex);
	if (S.IsEmpty()) return;

	TArray<int32> Changed;
	PC->Player_DropFromSlot(SlotIndex, S.Quantity, Changed);
	Close();
}

void UInvContextMenuWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	INV_CTX_LOG(Warning, TEXT("MouseLeave -> Close"));
	Close();
}

void UInvContextMenuWidget::Close()
{
	if (IsValid(OwnerMenu))
	{
		OwnerMenu->CloseContextMenu(); // call back
		return;
	}
	RemoveFromParent();
}

void UInvContextMenuWidget::RefreshSplitAmountText()
{
	if (!IsValid(Text_SplitAmount)) return;
	Text_SplitAmount->SetText(FText::AsNumber(SplitAmount));
}

void UInvContextMenuWidget::OnSplitSliderChanged(float Value01)
{
	if (SplitMax <= 1) return;

	SplitAmount = ClampSplitAmount(FMath::RoundToInt(Value01 * (float)SplitMax), SplitMax);

	RefreshSplitAmountText();
}

void UInvContextMenuWidget::OnSplitSpinChanged(float NewValue)
{
	if (SplitMax <= 1) return;

	SplitAmount = ClampSplitAmount(FMath::RoundToInt(NewValue), SplitMax);

	if (IsValid(SplitSlider))
	{
		SplitSlider->SetValue((SplitMax > 0) ? (float)SplitAmount / (float)SplitMax : 1.f);
	}

	RefreshSplitAmountText();
}

void UInvContextMenuWidget::Action_BeginSplit()
{

	INV_CTX_LOG(Warning, TEXT("Split 1"));
	if (!IsValid(OwnerMenu)) return;

	INV_CTX_LOG(Warning, TEXT("Split"));

	OwnerMenu->BeginSplitFrom(SlotIndex, SplitAmount);
	Close();
}

void UInvContextMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	INV_CTX_LOG(Warning, TEXT("BindWidget ptrs: Split=%d Drop1=%d DropAll=%d Consume=%d Slider=%d Spin=%d Text=%d"),
	IsValid(Button_Split) ? 1 : 0,
	IsValid(Button_DropOne) ? 1 : 0,
	IsValid(Button_DropAll) ? 1 : 0,
	IsValid(Button_Consume) ? 1 : 0,
	IsValid(SplitSlider) ? 1 : 0,
	IsValid(Text_SplitAmount) ? 1 : 0);

	if (IsValid(Button_Split))
		Button_Split->OnClicked.AddDynamic(this, &ThisClass::OnSplitClicked);

	if (IsValid(Button_DropOne))
		Button_DropOne->OnClicked.AddDynamic(this, &ThisClass::OnDropOneClicked);

	if (IsValid(Button_DropAll))
		Button_DropAll->OnClicked.AddDynamic(this, &ThisClass::OnDropAllClicked);

	if (IsValid(Button_Consume))
		Button_Consume->OnClicked.AddDynamic(this, &ThisClass::OnConsumeClicked);

	if (IsValid(SplitSlider))
		SplitSlider->OnValueChanged.AddDynamic(this, &ThisClass::OnSplitSliderChanged);

}

void UInvContextMenuWidget::OnSplitClicked()    { Action_BeginSplit(); }
void UInvContextMenuWidget::OnDropOneClicked()  { Action_DropOne(); }
void UInvContextMenuWidget::OnDropAllClicked()  { Action_DropAll(); }
void UInvContextMenuWidget::OnConsumeClicked()  { Action_ConsumeOne(); }