// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InvHighlightable.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UInvHighlightable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class INVENTORY_API IInvHighlightable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(BlueprintNativeEvent, Category = "Inventory")
	void Highlight();

	UFUNCTION(BlueprintNativeEvent, Category = "Inventory")
	void UnHighlight();
};
