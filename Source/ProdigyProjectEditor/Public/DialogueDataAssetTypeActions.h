#pragma once

#include "AssetTypeActions_Base.h"

class FDialogueDataAssetTypeActions : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override { return FText::FromString(TEXT("Dialogue Data Asset")); }
	virtual FColor GetTypeColor() const override { return FColor(80, 180, 255); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }

	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor) override;
};
