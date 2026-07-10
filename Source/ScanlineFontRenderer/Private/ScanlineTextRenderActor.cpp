// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScanlineTextRenderActor.h"
#include "ScanlineTextRenderComponent.h"
#include "Components/BillboardComponent.h"
#include "UObject/ConstructorHelpers.h"

AScanlineTextRenderActor::AScanlineTextRenderActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TextRenderComponent = CreateDefaultSubobject<UScanlineTextRenderComponent>(TEXT("TextRenderComponent"));
	RootComponent = TextRenderComponent;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (!IsRunningCommandlet() && (SpriteComponent != nullptr))
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> TextRenderTexture;
			FConstructorStatics()
				: TextRenderTexture(TEXT("/Engine/EditorResources/S_TextRenderActorIcon"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		SpriteComponent->Sprite = ConstructorStatics.TextRenderTexture.Get();
		SpriteComponent->SetRelativeScale3D_Direct(FVector(0.5f, 0.5f, 0.5f));
		SpriteComponent->SetupAttachment(TextRenderComponent);
		SpriteComponent->bIsScreenSizeScaled = true;
		SpriteComponent->SetUsingAbsoluteScale(true);
		SpriteComponent->bReceivesDecals = false;
	}

	// Make this actor visible in the editor
	bIsSpatiallyLoaded = false;
#endif
}
