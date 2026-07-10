// Copyright Nicholas Chalkley. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/TextProperty.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TextRenderComponent.h"
#include "ScanlineTextRenderComponent.generated.h"

class FPrimitiveSceneProxy;
class UScanlineFontFace;
class UMaterialInterface;

/**
 * Renders text in the world using a scanline font face (quadratic bezier curves).
 * Contains usual font related attributes such as Scale, Alignment, Color etc.
 */
UCLASS(Blueprintable, ClassGroup=Rendering, hidecategories=(Object,LOD,Physics,TextureStreaming,Activation,"Components|Activation",Collision), editinlinenew, meta=(BlueprintSpawnableComponent))
class SCANLINEFONTRENDERER_API UScanlineTextRenderComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	/** Text content, can be multi line using <br> as line separator */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text, meta=(MultiLine=true))
	FText Text;

	/** Text material */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	TObjectPtr<UMaterialInterface> TextMaterial;

	/** Scanline font face */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	TObjectPtr<UScanlineFontFace> FontFace;

	/** Horizontal text alignment */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text, meta=(DisplayName = "Horizontal Alignment"))
	TEnumAsByte<enum EHorizTextAligment> HorizontalAlignment;

	/** Vertical text alignment */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text, meta=(DisplayName = "Vertical Alignment"))
	TEnumAsByte<enum EVerticalTextAligment> VerticalAlignment;

	/** Color of the text, can be accessed as vertex color */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	FColor TextRenderColor;

	/** Horizontal scale, default is 1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	float XScale;

	/** Vertical scale, default is 1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	float YScale;

	/** Vertical size of the fonts largest character in world units. Transform, XScale and YScale will affect final size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	float WorldSize;

	/** Horizontal adjustment per character, default is 0.0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Text)
	float HorizSpacingAdjust;

	/** Vertical adjustment per character, default is 0.0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Text)
	float VertSpacingAdjust;

	/** Allows text to draw unmodified when using debug visualization modes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Rendering)
	uint32 bAlwaysRenderAsText:1;

	// -----------------------------

	/** Change the text value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	void SetText(const FText& Value);

	/** Change the text material and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	void SetTextMaterial(UMaterialInterface* Material);

	/** Change the font face and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	void SetFontFace(UScanlineFontFace* Value);

	/** Change the horizontal alignment and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	void SetHorizontalAlignment(EHorizTextAligment Value);

	/** Change the vertical alignment and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	void SetVerticalAlignment(EVerticalTextAligment Value);

	/** Change the text render color and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	void SetTextRenderColor(FColor Value);

	/** Change the text X scale and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	void SetXScale(float Value);

	/** Change the text Y scale and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	void SetYScale(float Value);

	/** Change the text horizontal spacing adjustment and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	void SetHorizSpacingAdjust(float Value);

	/** Change the text vertical spacing adjustment and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	void SetVertSpacingAdjust(float Value);

	/** Change the world size of the text and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	void SetWorldSize(float Value);

	/** Get local size of text */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	FVector2D GetTextLocalSize() const;

	/** Get world space size of text */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|ScanlineTextRender")
	FVector GetTextWorldSize() const;

	// -----------------------------

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial) override;
	virtual FMatrix GetRenderMatrix() const override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin USceneComponent Interface.
#if WITH_EDITOR
	virtual bool GetMaterialPropertyPath(int32 ElementIndex, UObject*& OutOwner, FString& OutPropertyPath, FProperty*& OutProperty) override;
	void OnFontPropertyChanged(UScanlineFontFace* FontFace, struct FPropertyChangedEvent& PropertyChangedEvent);
#endif // WITH_EDITOR
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void UpdateBounds() override;
	//~ End USceneComponent Interface.

	//~ Begin UActorComponent Interface.
	virtual bool RequiresGameThreadEndOfFrameUpdates() const override;
	virtual void PrecachePSOs() override;
	//~ End UActorComponent Interface.

	//~ Begin UObject Interface.
	virtual void PostLoad() override;
	//~ End UObject interface.

	virtual void OnRegister() override;

protected:
	/** Mark the render state as dirty */
	void MarkRenderStateDirty();

	/** Update the dynamic material instance with font textures */
	void UpdateDynamicMaterialInstance();

	/** Dynamic material instance created from TextMaterial with font textures set */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterialInstance;
};
