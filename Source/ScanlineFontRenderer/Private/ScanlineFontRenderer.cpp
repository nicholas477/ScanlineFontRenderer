// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScanlineFontRenderer.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FScanlineFontRendererModule"

void FScanlineFontRendererModule::StartupModule()
{
	// Map the plugin's shader directory
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ScanlineFontRenderer"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/ScanlineFontRenderer"), PluginShaderDir);

	UE_LOG(LogTemp, Log, TEXT("ScanlineFontRenderer: Registered shader directory at %s"), *PluginShaderDir);
}

void FScanlineFontRendererModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FScanlineFontRendererModule, ScanlineFontRenderer)