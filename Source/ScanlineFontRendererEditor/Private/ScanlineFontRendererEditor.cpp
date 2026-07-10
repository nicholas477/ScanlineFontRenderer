// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScanlineFontRendererEditor.h"
#include "AssetTypeActions_ScanlineFontFace.h"
#include "ScanlineFontFaceFactory.h"
#include "ScanlineFontFace.h"
#include "Engine/FontFace.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserMenuContexts.h"
#include "ToolMenus.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "FScanlineFontRendererEditorModule"

void FScanlineFontRendererEditorModule::StartupModule()
{
	RegisterAssetTypeActions();
	RegisterMenuExtensions();
}

void FScanlineFontRendererEditorModule::ShutdownModule()
{
	UnregisterMenuExtensions();
	UnregisterAssetTypeActions();
}

void FScanlineFontRendererEditorModule::RegisterAssetTypeActions()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Register ScanlineFontFace asset type actions
	TSharedPtr<IAssetTypeActions> ScanlineFontFaceActions = MakeShared<FAssetTypeActions_ScanlineFontFace>();
	AssetTools.RegisterAssetTypeActions(ScanlineFontFaceActions.ToSharedRef());
	RegisteredAssetTypeActions.Add(ScanlineFontFaceActions);
}

void FScanlineFontRendererEditorModule::UnregisterAssetTypeActions()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (TSharedPtr<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
		}
	}
	RegisteredAssetTypeActions.Empty();
}

void FScanlineFontRendererEditorModule::RegisterMenuExtensions()
{
	if (!UToolMenus::IsToolMenuUIEnabled())
	{
		return;
	}

	// Register content browser context menu extension for UFontFace
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.FontFace");
	if (Menu)
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddDynamicEntry("CreateScanlineFontFace", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
			if (!Context || Context->SelectedAssets.Num() == 0)
			{
				return;
			}

			InSection.AddMenuEntry(
				"CreateScanlineFontFace",
				LOCTEXT("CreateScanlineFontFace", "Create Scanline Font Face"),
				LOCTEXT("CreateScanlineFontFaceTooltip", "Creates a new Scanline Font Face asset from this UFontFace"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Context]()
				{
					// Get the asset tools module
					IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

					for (const FAssetData& AssetData : Context->SelectedAssets)
					{
						UFontFace* FontFace = Cast<UFontFace>(AssetData.GetAsset());
						if (!FontFace)
						{
							continue;
						}

						// Determine the package path for the new asset
						FString PackagePath = FPackageName::GetLongPackagePath(AssetData.PackageName.ToString());
						FString AssetName = FString::Printf(TEXT("%s_Scanline"), *AssetData.AssetName.ToString());

						// Create the factory and configure it
						UScanlineFontFaceFactory* Factory = NewObject<UScanlineFontFaceFactory>();
						Factory->SourceFontFace = FontFace;
						Factory->FontSize = 72.0f;

						// Create the new asset
						FString UniquePackageName;
						FString UniqueAssetName;
						AssetTools.CreateUniqueAssetName(PackagePath / AssetName, TEXT(""), UniquePackageName, UniqueAssetName);

						if (UObject* NewAsset = AssetTools.CreateAsset(UniqueAssetName, PackagePath, UScanlineFontFace::StaticClass(), Factory))
						{
							// Sync content browser to the new asset
							TArray<UObject*> ObjectsToSync;
							ObjectsToSync.Add(NewAsset);
							FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
							ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);

							UE_LOG(LogTemp, Log, TEXT("Created Scanline Font Face: %s"), *NewAsset->GetPathName());
						}
					}
				}))
			);
		}));
	}
}

void FScanlineFontRendererEditorModule::UnregisterMenuExtensions()
{
	if (UToolMenus::IsToolMenuUIEnabled() && UToolMenus::Get())
	{
		UToolMenus::Get()->RemoveMenu("ContentBrowser.AssetContextMenu.FontFace");
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FScanlineFontRendererEditorModule, ScanlineFontRendererEditor)
