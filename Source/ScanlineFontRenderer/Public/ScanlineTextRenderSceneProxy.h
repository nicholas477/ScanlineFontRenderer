// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"
#include "Materials/MaterialRelevance.h"
#include "Components/TextRenderComponent.h"

class UScanlineTextRenderComponent;
class UScanlineFontFace;
class UMaterialInterface;

/**
 * Scene proxy for scanline text rendering.
 * Generates mesh geometry from UScanlineFontFace Bezier curve data.
 */
class FScanlineTextRenderSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FScanlineTextRenderSceneProxy(UScanlineTextRenderComponent* Component);
	virtual ~FScanlineTextRenderSceneProxy();

	// Begin FPrimitiveSceneProxy interface
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override;
	virtual uint32 GetMemoryFootprint() const override;
	uint32 GetAllocatedSize() const;
	// End FPrimitiveSceneProxy interface

private:
	// Begin FPrimitiveSceneProxy interface
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	// End FPrimitiveSceneProxy interface

	void ReleaseRenderThreadResources();

	/**
	 * Build mesh from text string and scanline font face.
	 * Flattens Bezier curves into triangulated geometry.
	 */
	bool BuildStringMesh(TArray<FDynamicMeshVertex>& OutVertices, TArray<uint32>& OutIndices);

	/**
	 * Builds a quad for a single character glyph.
	 */
	void BuildGlyphMesh(
		const struct FScanlineFontGlyph* Glyph,
		const FVector2D& Position,
		const FVector2D& GlyphIndexUV,
		const FVector2D& Scale,
		const FVector2D& Padding,
		TArray<FDynamicMeshVertex>& OutVertices,
		TArray<uint32>& OutIndices);

private:
	struct FTextBatch
	{
		int32 IndexBufferOffset;
		int32 IndexBufferCount;

		int32 VertexBufferOffset;
		int32 VertexBufferCount;

		const UMaterialInterface* Material;
	};

	// Vertex and index buffers
	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;
	TArray<FTextBatch> TextBatches;

	// Material and rendering
	const FColor TextRenderColor;
	FMaterialRelevance MaterialRelevance;
	UMaterialInterface* TextMaterial;

	// Font and text data
	UScanlineFontFace* FontFace;
	FText Text;
	float XScale;
	float YScale;
	float WorldSize;
	float HorizSpacingAdjust;
	float VertSpacingAdjust;
	TEnumAsByte<EHorizTextAligment> HorizontalAlignment;
	TEnumAsByte<EVerticalTextAligment> VerticalAlignment;
	bool bAlwaysRenderAsText;
};
