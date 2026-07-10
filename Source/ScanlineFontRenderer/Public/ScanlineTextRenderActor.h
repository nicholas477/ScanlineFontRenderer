// Copyright Nicholas Chalkley. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ScanlineTextRenderActor.generated.h"

class UScanlineTextRenderComponent;
class UBillboardComponent;

/**
 * Actor that renders 3D text using scanline font rendering.
 * Provides a simple way to place scanline text in the world.
 */
UCLASS(ComponentWrapperClass, hidecategories=(Input), showCategories=("Input|MouseInput", "Input|TouchInput"), ConversionRoot, meta=(ChildCanTick))
class SCANLINEFONTRENDERER_API AScanlineTextRenderActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	/** Component to render the text */
	UPROPERTY(Category = ScanlineTextRenderActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Rendering|Components|ScanlineTextRender", AllowPrivateAccess = "true"))
	TObjectPtr<UScanlineTextRenderComponent> TextRenderComponent;

#if WITH_EDITORONLY_DATA
	// Reference to the billboard component
	UPROPERTY()
	TObjectPtr<UBillboardComponent> SpriteComponent;
#endif

public:
	/** Returns TextRenderComponent subobject */
	UScanlineTextRenderComponent* GetTextRender() const { return TextRenderComponent; }

#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }
#endif
};
