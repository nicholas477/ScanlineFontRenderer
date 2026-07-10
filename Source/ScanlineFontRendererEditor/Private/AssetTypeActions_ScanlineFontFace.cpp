// Copyright Nicholas Chalkley. All Rights Reserved.

#include "AssetTypeActions_ScanlineFontFace.h"
#include "ScanlineFontFace.h"
#include "Engine/FontFace.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_ScanlineFontFace"

FText FAssetTypeActions_ScanlineFontFace::GetName() const
{
	return LOCTEXT("AssetTypeActions_ScanlineFontFace", "Scanline Font Face");
}

FColor FAssetTypeActions_ScanlineFontFace::GetTypeColor() const
{
	return FColor(128, 192, 255); // Light blue
}

UClass* FAssetTypeActions_ScanlineFontFace::GetSupportedClass() const
{
	return UScanlineFontFace::StaticClass();
}

uint32 FAssetTypeActions_ScanlineFontFace::GetCategories()
{
	return EAssetTypeCategories::UI;
}

void FAssetTypeActions_ScanlineFontFace::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	TArray<TWeakObjectPtr<UScanlineFontFace>> FontFaces = GetTypedWeakObjectPtrs<UScanlineFontFace>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ScanlineFontFace_Reimport", "Reimport from Source"),
		LOCTEXT("ScanlineFontFace_ReimportTooltip", "Reimport this scanline font face from its source UFontFace asset"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_ScanlineFontFace::ExecuteReimport, FontFaces),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_ScanlineFontFace::CanExecuteReimport, FontFaces)
		)
	);
}

void FAssetTypeActions_ScanlineFontFace::ExecuteReimport(TArray<TWeakObjectPtr<UScanlineFontFace>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UScanlineFontFace* FontFace = (*ObjIt).Get())
		{
			FontFace->ImportFromFontFace(FontFace->SourceFontFace.LoadSynchronous());
		}
	}
}

bool FAssetTypeActions_ScanlineFontFace::CanExecuteReimport(TArray<TWeakObjectPtr<UScanlineFontFace>> Objects) const
{
	// TODO: Check if source UFontFace is available
	return Objects.Num() > 0;
}

#undef LOCTEXT_NAMESPACE
