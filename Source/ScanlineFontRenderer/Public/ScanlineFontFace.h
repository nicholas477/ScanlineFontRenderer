// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ScanlineFontFace.generated.h"

/**
 * A single quadratic bezier curve segment.
 *
 * The scanline renderer ONLY works with quadratic bezier curves, so all curves
 * (linear, quadratic, cubic) are converted to these.
 */
USTRUCT(BlueprintType)
struct SCANLINEFONTRENDERER_API FScanlineQuadraticBezier
{
	GENERATED_BODY()

	/** Start point of the curve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bezier")
	FVector2D P0 = FVector2D::ZeroVector;

	/** Control point of the curve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bezier")
	FVector2D P1 = FVector2D::ZeroVector;

	/** End point of the curve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bezier")
	FVector2D P2 = FVector2D::ZeroVector;

	FScanlineQuadraticBezier() = default;

	FScanlineQuadraticBezier(const FVector2D& InP0, const FVector2D& InP1, const FVector2D& InP2)
		: P0(InP0), P1(InP1), P2(InP2)
	{}

	/**
	 * Evaluate the bezier curve at parameter t [0,1]
	 */
	FVector2D Evaluate(float T) const
	{
		const float OneMinusT = 1.0f - T;
		return (OneMinusT * OneMinusT * P0) + (2.0f * OneMinusT * T * P1) + (T * T * P2);
	}

	/**
	 * Get the derivative (tangent) at parameter t [0,1]
	 */
	FVector2D GetDerivative(float T) const
	{
		return 2.0f * ((1.0f - T) * (P1 - P0) + T * (P2 - P1));
	}

	/**
	 * Returns if the curve is a straight horizontal line. 
	 * Straight horizontal lines are discarded in the font textures.
	 */
	bool IsHorizontal() const
	{
		return FMath::IsNearlyEqual(P0.Y, P1.Y) && FMath::IsNearlyEqual(P1.Y, P2.Y);
	}

	/**
	 * Get the bounding box of this curve
	 */
	FBox2D GetBounds() const;

	/**
	 * Subdivide the curve at parameter t into two curves
	 */
	void Subdivide(float T, FScanlineQuadraticBezier& OutLeft, FScanlineQuadraticBezier& OutRight) const;

	/*
	 * Returns the 2 T values for the X and Y inflection values
	 */
	TPair<float, float> GetTurningPoints() const
	{
		// tx = (P0.X - P1.X) / (P0.X - (2 * P1.X) + P2.X)
		// ty = (P0.Y - P1.Y) / (P0.Y - (2 * P1.Y) + P2.Y)

		return {
			(P0.X - P1.X) / (P0.X - (2 * P1.X) + P2.X),
			(P0.Y - P1.Y) / (P0.Y - (2 * P1.Y) + P2.Y)
		};
	}

	TPair<FFloat16Color, FFloat16Color> ToColorPair() const
	{
		TPair<FFloat16Color, FFloat16Color> OutPair;

		OutPair.Key.R = FFloat16(P0.X);
		OutPair.Key.G = FFloat16(P0.Y);
		OutPair.Key.B = FFloat16(P1.X);

		OutPair.Value.R = FFloat16(P1.Y);
		OutPair.Value.G = FFloat16(P2.X);
		OutPair.Value.B = FFloat16(P2.Y);

		return OutPair;
	}
};

/**
 * A closed contour made up of quadratic bezier curves
 */
USTRUCT(BlueprintType)
struct SCANLINEFONTRENDERER_API FScanlineFontContour
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font")
	TArray<FScanlineQuadraticBezier> Curves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font")
	bool bClockwise = true;

	FScanlineFontContour() = default;

	/**
	 * Get the bounding box of this entire contour
	 */
	FBox2D GetBounds() const;

	/**
	 * Check if this contour is closed (last curve ends at first curve's start)
	 */
	bool IsClosed() const;

	/**
	 * Reverse the direction of this contour
	 */
	void Reverse();
};

/**
 * A single glyph represented as quadratic bezier curves
 */
USTRUCT(BlueprintType)
struct SCANLINEFONTRENDERER_API FScanlineFontGlyph
{
	GENERATED_BODY()

	/** Unicode code point this glyph represents */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font")
	int32 CodePoint = 0;

	/** Array of contours forming this glyph */
	UPROPERTY(BlueprintReadWrite, Category = "Font")
	TArray<FScanlineFontContour> Contours;

	/** Advance width in font units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font")
	float AdvanceWidth = 0.0f;

	/** Left side bearing in font units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font")
	float LeftSideBearing = 0.0f;

	/** Bounding box in font units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font")
	FBox2D BoundingBox = FBox2D(ForceInit);

	FScanlineFontGlyph() = default;

	/**
	 * Get the total number of curves in all contours
	 */
	int32 GetTotalCurveCount() const;

	/**
	 * Check if this glyph has any geometry
	 */
	bool IsEmpty() const { return Contours.Num() == 0; }

	/**
	 * Transform all curves by a 2D transform
	 */
	void Transform(const FMatrix2x2& Matrix, const FVector2D& Translation);

	/**
	 * Scale all curves uniformly
	 */
	void Scale(float ScaleFactor);
};

#if WITH_EDITOR
class UScanlineFontFace;
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnScanlineFontFacePropertyChanged, UScanlineFontFace*, struct FPropertyChangedEvent&);
#endif

/**
 * Font face object containing only quadratic bezier curve data for scanline rendering
 * This is a lightweight representation suitable for rasterization
 */
UCLASS(BlueprintType)
class SCANLINEFONTRENDERER_API UScanlineFontFace : public UObject
{
	GENERATED_BODY()

public:
	UScanlineFontFace(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Font")
	FString CharactersToImport;

	/** Font family name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font")
	FString FamilyName;

	/** Font style name (Regular, Bold, Italic, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font")
	FString StyleName;

	// Padding (in font units) around glyph quads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font")
	float Padding = 1.f;

	/** Units per em (typically 1000 or 2048) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font|Font Metrics")
	int32 UnitsPerEm = 1000;

	/** Ascender height in font units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font|Font Metrics")
	float Ascender = 800.0f;

	/** Descender height in font units (typically negative) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font|Font Metrics")
	float Descender = -200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font|Font Metrics")
	float Height = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font|Font Metrics")
	float MaxAdvanceWidth = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font|Font Metrics")
	float MaxAdvanceHeight = 0.f;

	/** Line gap in font units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Font|Font Metrics")
	float LineGap = 0.0f;

	/** Map of code points to glyphs */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Font|Glyphs")
	TMap<int32, FScanlineFontGlyph> Glyphs;

	/** Map of code points to glyph indices */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Font|Glyphs")
	TMap<int32, int32> CodePointToIndex;

	/** Map of code points to glyph indices in the glyph texture.
	 * Some glyphs won't have indicies (like space, whitespace glyphs)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Font|Glyphs")
	TMap<int32, int32> CodePointToTextureIndex;

	/** 
	 * Texture containing Bezier curve data in float16 RGB format.
	 * Each quadratic bezier curve (P0, P1, P2) uses 2 pixels:
	 * Pixel 0: R=P0.x, G=P0.y, B=P1.x
	 * Pixel 1: R=P1.y, G=P2.x, B=P2.y
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Font|Textures")
	TObjectPtr<class UTexture2D> CurveDataTexture;

	/**
	 * Texture containing glyph curve index ranges in RGBA32F format.
	 * Each pixel represents one glyph (indexed by code point):
	 * R = starting curve index (reinterpreted int32 as float)
	 * G = end curve index + 1 (reinterpreted int32 as float)
	 * B = Glyph size X
	 * A = Glyph size y
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Font|Textures")
	TObjectPtr<class UTexture2D> GlyphIndexTexture;

public:
	/**
	 * Get a glyph by code point
	 */
	UFUNCTION(BlueprintCallable, Category = "Font")
	bool GetGlyph(int32 CodePoint, FScanlineFontGlyph& OutGlyph) const;

	UFUNCTION(BlueprintPure, Category = "Font")
	int32 GetIndexOfCodepoint(int32 CodePoint) const;

	UFUNCTION(BlueprintPure, Category = "Font")
	int32 GetIndexOfTextureCodepoint(int32 CodePoint) const;
	
	/*
	 * Gets the UV index of a codepoint in the glyph index texture
	 */
	UFUNCTION(BlueprintPure, Category = "Font")
	bool GetUVForCodepoint(int32 CodePoint, FVector2D& OutUV) const;

	/**
	 * Check if a glyph exists for the given code point
	 */
	UFUNCTION(BlueprintCallable, Category = "Font")
	bool HasGlyph(int32 CodePoint) const;

	/**
	 * Get all available code points
	 */
	UFUNCTION(BlueprintCallable, Category = "Font")
	void GetAvailableCodePoints(TArray<int32>& OutCodePoints) const;

	/**
	 * Get the total number of glyphs
	 */
	UFUNCTION(BlueprintCallable, Category = "Font")
	int32 GetGlyphCount() const { return Glyphs.Num(); }

	/**
	 * Scale factor to convert font units to pixels at a given point size
	 */
	UFUNCTION(BlueprintCallable, Category = "Font")
	float GetScaleForPixelHeight(float PixelHeight) const;

	UFUNCTION(BlueprintPure, Category = "Font")
	FVector2D CalculateEmToWorldScale(float ScaleX, float ScaleY, float WorldSize) const;

	/**
	 * Get the line height in font units
	 */
	UFUNCTION(BlueprintPure, Category = "Font")
	float GetLineHeight() const;

	//UFUNCTION(BlueprintPure, Category = "Font")
	//float GetMaxCharHeight() const;

	/**
	 * Calculate the width of a text string in font units
	 */
	UFUNCTION(BlueprintCallable, Category = "Font")
	float MeasureText(const FString& Text) const;

	/**
	 * Clear all glyphs + texture data
	 */
	UFUNCTION(BlueprintCallable, Category = "Font")
	void Clear();

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = "Font")
	void RebuildTextureData();

	/**
	 * Import from a UFontFace asset (converts cubic beziers to quadratic)
	 * @param FontFace - The UFontFace asset to import from
	 * @param FontSize - The font size to use for extraction
	 * @return True if import was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Font")
	bool ImportFromFontFace(class UFontFace* FontFace, float FontSize = 72.0f);

	/**
	 * Import specific code points from a UFontFace asset
	 * @param FontFace - The UFontFace asset to import from
	 * @param CodePoints - Array of Unicode code points to import
	 * @param FontSize - The font size to use for extraction
	 * @return Number of glyphs successfully imported
	 */
	UFUNCTION(BlueprintCallable, Category = "Font")
	int32 ImportCodePointsFromFontFace(class UFontFace* FontFace, const TArray<int32>& CodePoints, float FontSize = 72.0f);

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
	static FOnScanlineFontFacePropertyChanged OnPropertyChanged;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UFontFace> SourceFontFace;
#endif
};
