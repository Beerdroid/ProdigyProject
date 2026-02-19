#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

FORCEINLINE FVector2D ClampWidgetPosToViewport(
	const FVector2D& DesiredPos,
	const FVector2D& WidgetSize,
	const FVector2D& ViewportSize,
	const FVector2D& PaddingPx)
{
	FVector2D P = DesiredPos;

	P.X = FMath::Max(PaddingPx.X, P.X);
	P.Y = FMath::Max(PaddingPx.Y, P.Y);

	P.X = FMath::Min(P.X, ViewportSize.X - WidgetSize.X - PaddingPx.X);
	P.Y = FMath::Min(P.Y, ViewportSize.Y - WidgetSize.Y - PaddingPx.Y);

	return P;
}

FORCEINLINE void PlaceSingleWidgetBottomCenter(
	APlayerController* PC,
	UUserWidget* Widget,
	float BottomPaddingPx = 32.f,
	FVector2D PaddingPx = FVector2D(16.f, 16.f),
	bool bRemoveDPIScale = true)
{
	if (!IsValid(PC) || !IsValid(Widget)) return;

	int32 VX = 0, VY = 0;
	PC->GetViewportSize(VX, VY);
	if (VX <= 0 || VY <= 0) return;

	const FVector2D ViewportSize((float)VX, (float)VY);

	Widget->ForceLayoutPrepass();
	const FVector2D Size = Widget->GetDesiredSize();
	if (Size.IsNearlyZero()) return;

	const float CenterX = ViewportSize.X * 0.5f;
	FVector2D Pos(CenterX - Size.X * 0.5f, ViewportSize.Y - BottomPaddingPx - Size.Y);

	Pos = ClampWidgetPosToViewport(Pos, Size, ViewportSize, PaddingPx);

	Widget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
	Widget->SetPositionInViewport(Pos, bRemoveDPIScale);
}
