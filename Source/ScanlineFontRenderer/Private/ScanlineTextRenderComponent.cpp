// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScanlineTextRenderComponent.h"
#include "ScanlineTextRenderSceneProxy.h"
#include "ScanlineFontFace.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ScanlineTextRenderSceneProxyUtils.h"
#include "Engine/Engine.h"

UE_DISABLE_OPTIMIZATION

UScanlineTextRenderComponent::UScanlineTextRenderComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default values
	Text = NSLOCTEXT("ScanlineTextRender", "DefaultText", "Text");
	HorizontalAlignment = EHTA_Left;
	VerticalAlignment = EVRTA_TextBottom;
	TextRenderColor = FColor::White;
	XScale = 1.0f;
	YScale = 1.0f;
	WorldSize = 32.0f;
	HorizSpacingAdjust = 0.0f;
	VertSpacingAdjust = 0.0f;
	bAlwaysRenderAsText = false;

	// Component settings
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	CastShadow = false;
	bUseAsOccluder = false;

	if (!IsRunningDedicatedServer())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UScanlineFontFace> Font;
			ConstructorHelpers::FObjectFinderOptional<UMaterial> TextMaterial;
			FConstructorStatics()
				: Font(TEXT("/ScanlineFontRenderer/RobotoBold_Scanline"))
				, TextMaterial(TEXT("/ScanlineFontRenderer/M_ScanlineFont"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		FontFace = ConstructorStatics.Font.Get();
		TextMaterial = ConstructorStatics.TextMaterial.Get();
	}
}

void UScanlineTextRenderComponent::SetText(const FText& Value)
{
	Text = Value;
	MarkRenderStateDirty();
}

void UScanlineTextRenderComponent::SetTextMaterial(UMaterialInterface* Material)
{
	TextMaterial = Material;
	UpdateDynamicMaterialInstance();
	MarkRenderStateDirty();
}

void UScanlineTextRenderComponent::SetFontFace(UScanlineFontFace* Value)
{
	FontFace = Value;
	UpdateDynamicMaterialInstance();
	MarkRenderStateDirty();
}

void UScanlineTextRenderComponent::SetHorizontalAlignment(EHorizTextAligment Value)
{
	HorizontalAlignment = Value;
	MarkRenderStateDirty();
}

void UScanlineTextRenderComponent::SetVerticalAlignment(EVerticalTextAligment Value)
{
	VerticalAlignment = Value;
	MarkRenderStateDirty();
}

void UScanlineTextRenderComponent::SetTextRenderColor(FColor Value)
{
	TextRenderColor = Value;
	MarkRenderStateDirty();
}

void UScanlineTextRenderComponent::SetXScale(float Value)
{
	XScale = Value;
	MarkRenderStateDirty();
}

void UScanlineTextRenderComponent::SetYScale(float Value)
{
	YScale = Value;
	MarkRenderStateDirty();
}

void UScanlineTextRenderComponent::SetHorizSpacingAdjust(float Value)
{
	HorizSpacingAdjust = Value;
	MarkRenderStateDirty();
}

void UScanlineTextRenderComponent::SetVertSpacingAdjust(float Value)
{
	VertSpacingAdjust = Value;
	MarkRenderStateDirty();
}

void UScanlineTextRenderComponent::SetWorldSize(float Value)
{
	WorldSize = Value;
	MarkRenderStateDirty();
}

FVector2D UScanlineTextRenderComponent::GetTextLocalSize() const
{
	if (!Text.IsEmpty() && FontFace)
	{
		float SizeY = 0.f;
		float SizeX = 0.f;

		const FVector2D EmToWorldScale = FontFace->CalculateEmToWorldScale(XScale, YScale, WorldSize);

		FTextIterator It(*Text.ToString());
		while (It.NextLine())
		{
			const FVector2D LineSize = ComputeTextSize(It, FontFace, EmToWorldScale.X, EmToWorldScale.Y, HorizSpacingAdjust, VertSpacingAdjust);
			SizeY += LineSize.Y > 0.f ? LineSize.Y : FontFace->GetLineHeight();
			SizeX = FMath::Max(SizeX, LineSize.X);

			TCHAR Ch = 0;
			while (It.NextCharacterInLine(Ch)) {}
		}

		return FVector2D(SizeX, SizeY);
	}

	return FVector2D(0.f, 0.f);
}

FVector UScanlineTextRenderComponent::GetTextWorldSize() const
{
	FVector2D LocalSize = GetTextLocalSize();
	FTransform LocalToWorld = GetComponentTransform();
	return LocalToWorld.TransformVector(FVector(LocalSize.X, 0.f, LocalSize.Y));
}

FPrimitiveSceneProxy* UScanlineTextRenderComponent::CreateSceneProxy()
{
	UpdateDynamicMaterialInstance();
	return new FScanlineTextRenderSceneProxy(this);
}

void UScanlineTextRenderComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (DynamicMaterialInstance)
	{
		OutMaterials.AddUnique(DynamicMaterialInstance);
	}
	else if (TextMaterial)
	{
		OutMaterials.AddUnique(TextMaterial);
	}
}

int32 UScanlineTextRenderComponent::GetNumMaterials() const
{
	return (DynamicMaterialInstance || TextMaterial) ? 1 : 0;
}

UMaterialInterface* UScanlineTextRenderComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex == 0)
	{
		return DynamicMaterialInstance ? static_cast<UMaterialInterface*>(DynamicMaterialInstance.Get()) : TextMaterial.Get();
	}
	return nullptr;
}

bool UScanlineTextRenderComponent::ShouldRecreateProxyOnUpdateTransform() const
{
	return false;
}

void UScanlineTextRenderComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial)
{
	if (ElementIndex == 0)
	{
		SetTextMaterial(InMaterial);
	}
}

FMatrix UScanlineTextRenderComponent::GetRenderMatrix() const
{
	// Adjust LocalToWorld transform to account for vertical text alignment when rendering.
	if (!Text.IsEmpty() && FontFace)
	{
		FMatrix VerticalTransform = FMatrix::Identity;
		const float VerticalAlignmentOffset = -ComputeVerticalAlignmentOffset(GetTextLocalSize().Y, VerticalAlignment);
		VerticalTransform = VerticalTransform.ConcatTranslation(FVector(0.f, 0.f, VerticalAlignmentOffset));

		return VerticalTransform * GetComponentTransform().ToMatrixWithScale();
	}
	return UPrimitiveComponent::GetRenderMatrix();
}

#if WITH_EDITOR
bool UScanlineTextRenderComponent::GetMaterialPropertyPath(int32 ElementIndex, UObject*& OutOwner, FString& OutPropertyPath, FProperty*& OutProperty)
{
	if (ElementIndex == 0)
	{
		OutOwner = this;
		OutPropertyPath = TEXT("TextMaterial");
		OutProperty = FindFProperty<FProperty>(GetClass(), TEXT("TextMaterial"));
		return true;
	}
	return false;
}

void UScanlineTextRenderComponent::OnFontPropertyChanged(UScanlineFontFace* InFontFace, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (InFontFace == this->FontFace)
	{
		MarkRenderStateDirty();
	}
}
#endif

FBoxSphereBounds UScanlineTextRenderComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FVector2D LocalSize = GetTextLocalSize() + (FontFace ? (FVector2D(FontFace->Padding * 2.f) * FontFace->CalculateEmToWorldScale(XScale, YScale, WorldSize)) : FVector2D(0.f));

	// Create bounds based on text alignment
	// Note: Vertices use coordinate system (0, Y, Z) where Y is horizontal and Z is vertical
	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;

	// Horizontal alignment (Y axis in vertex space)
	switch (HorizontalAlignment)
	{
	case EHTA_Center:
		Origin.Y = -LocalSize.X * 0.5f;
		Extent.Y = LocalSize.X * 0.5f;
		break;
	case EHTA_Right:
		Origin.Y = 0.f;
		Extent.Y = LocalSize.X;
		break;
	case EHTA_Left:
	default:
		Origin.Y = -LocalSize.X;
		Extent.Y = 0.f;
		break;
	}

	// Vertical alignment (Z axis in vertex space)
	switch (VerticalAlignment)
	{
	case EVRTA_TextCenter:
		Origin.Z = -LocalSize.Y * 0.5f;
		Extent.Z = LocalSize.Y * 0.5f;
		break;
	case EVRTA_QuadTop:
	case EVRTA_TextTop:
		Origin.Z = -LocalSize.Y;
		Extent.Z = 0.0f;
		break;
	case EVRTA_TextBottom:
	default:
		Origin.Z = 0.0f;
		Extent.Z = LocalSize.Y;
		break;
	}

	// X is always 0 for flat text
	Origin.X = 0.0f;
	Extent.X = 1.0f; // Small extent on X axis

	FBox LocalBox(Origin, Extent);
	FBoxSphereBounds LocalBounds(LocalBox);
	return LocalBounds.TransformBy(LocalToWorld);
}

void UScanlineTextRenderComponent::UpdateBounds()
{
	FTransform LocalToWorld = GetComponentTransform();
	Bounds = CalcBounds(LocalToWorld);
}

bool UScanlineTextRenderComponent::RequiresGameThreadEndOfFrameUpdates() const
{
	return false;
}

void UScanlineTextRenderComponent::PrecachePSOs()
{
	// TODO: Implement PSO precaching for scanline text rendering
}

void UScanlineTextRenderComponent::PostLoad()
{
	Super::PostLoad();

	// Update bounds after loading
	UpdateBounds();

#if WITH_EDITOR
	UScanlineFontFace::OnPropertyChanged.AddUObject(this, &ThisClass::OnFontPropertyChanged);
#endif
}

void UScanlineTextRenderComponent::OnRegister()
{
	Super::OnRegister();
}

void UScanlineTextRenderComponent::MarkRenderStateDirty()
{
	Super::MarkRenderStateDirty();
	UpdateBounds();
}

void UScanlineTextRenderComponent::UpdateDynamicMaterialInstance()
{
	// Clear dynamic material if we don't have a base material
	if (!TextMaterial)
	{
		DynamicMaterialInstance = nullptr;
		return;
	}

	// Create or reuse dynamic material instance
	if (!DynamicMaterialInstance || DynamicMaterialInstance->Parent != TextMaterial)
	{
		DynamicMaterialInstance = UMaterialInstanceDynamic::Create(TextMaterial, this);
	}

	// Set texture parameters from font face
	if (DynamicMaterialInstance && FontFace)
	{
		if (FontFace->CurveDataTexture)
		{
			DynamicMaterialInstance->SetTextureParameterValue(FName("CurveDataTexture"), FontFace->CurveDataTexture);
		}

		if (FontFace->GlyphIndexTexture)
		{
			DynamicMaterialInstance->SetTextureParameterValue(FName("GlyphIndexTexture"), FontFace->GlyphIndexTexture);
		}
	}
}

UE_ENABLE_OPTIMIZATION
