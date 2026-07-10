// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScanlineFontFaceFactory.h"
#include "ScanlineFontFace.h"
#include "Engine/FontFace.h"

UScanlineFontFaceFactory::UScanlineFontFaceFactory()
{
	SupportedClass = UScanlineFontFace::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UScanlineFontFaceFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UScanlineFontFace* NewFontFace = NewObject<UScanlineFontFace>(InParent, InClass, InName, Flags);

	// If we have a source font face, import from it
	if (SourceFontFace)
	{
		NewFontFace->ImportFromFontFace(SourceFontFace, FontSize);

		// Reset for next creation
		SourceFontFace = nullptr;
		FontSize = 72.0f;
	}

	return NewFontFace;
}
