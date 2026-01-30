#include "DialogueDataAssetTypeActions.h"

#include "DialogueSystem/DialogueDataAsset.h"
#include "FDialogueAssetEditorToolkit.h"

UClass* FDialogueDataAssetTypeActions::GetSupportedClass() const
{
	return UDialogueDataAsset::StaticClass();
}

void FDialogueDataAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	UE_LOG(LogTemp, Warning, TEXT("DialogueDataAssetTypeActions::OpenAssetEditor called (%d objects)"), InObjects.Num());

	for (UObject* Obj : InObjects)
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> %s (%s)"), *GetNameSafe(Obj), *GetNameSafe(Obj ? Obj->GetClass() : nullptr));

		if (UDialogueDataAsset* Asset = Cast<UDialogueDataAsset>(Obj))
		{
			TSharedRef<FDialogueAssetEditorToolkit> Editor = MakeShared<FDialogueAssetEditorToolkit>();

			const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
				? EToolkitMode::WorldCentric
				: EToolkitMode::Standalone;

			Editor->InitDialogueEditor(Mode, EditWithinLevelEditor, Asset);
		}
	}
}
