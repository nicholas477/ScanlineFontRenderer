// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScanlineTextRenderSceneProxy.h"
#include "ScanlineTextRenderSceneProxyUtils.h"
#include "ScanlineTextRenderComponent.h"
#include "ScanlineFontFace.h"
#include "SceneView.h"
#include "SceneManagement.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "PrimitiveViewRelevance.h"

UE_DISABLE_OPTIMIZATION

FScanlineTextRenderSceneProxy::FScanlineTextRenderSceneProxy(UScanlineTextRenderComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, VertexFactory(GetScene().GetFeatureLevel(), "FScanlineTextRenderSceneProxy")
	, TextRenderColor(Component->TextRenderColor)
	, FontFace(Component->FontFace)
	, Text(Component->Text)
	, XScale(Component->XScale)
	, YScale(Component->YScale)
	, WorldSize(Component->WorldSize)
	, HorizSpacingAdjust(Component->HorizSpacingAdjust)
	, VertSpacingAdjust(Component->VertSpacingAdjust)
	, HorizontalAlignment(Component->HorizontalAlignment)
	, VerticalAlignment(Component->VerticalAlignment)
	, bAlwaysRenderAsText(Component->bAlwaysRenderAsText)
{
	SetWireframeColor(FLinearColor(0.f, 1.f, 0.f));

	// Get effective material (this will be the dynamic material instance if available)
	UMaterialInterface* EffectiveMaterial = Component->GetMaterial(0);

	if (EffectiveMaterial)
	{
		const UMaterial* BaseMaterial = EffectiveMaterial->GetMaterial_Concurrent();
		if (BaseMaterial->MaterialDomain != MD_Surface && BaseMaterial->MaterialDomain != MD_DeferredDecal)
		{
			EffectiveMaterial = nullptr;
		}
	}

	if (!EffectiveMaterial)
	{
		EffectiveMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	TextMaterial = EffectiveMaterial;
	MaterialRelevance |= TextMaterial->GetMaterial_Concurrent()->GetRelevance(GetScene().GetShaderPlatform());

	bVerifyUsedMaterials = false;
}

FScanlineTextRenderSceneProxy::~FScanlineTextRenderSceneProxy()
{
	ReleaseRenderThreadResources();
}

void FScanlineTextRenderSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	if (!FontFace || Text.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("FScanlineTextRenderSceneProxy::CreateRenderThreadResources - FontFace=%s, Text.IsEmpty=%d"), 
			FontFace ? TEXT("Valid") : TEXT("NULL"), Text.IsEmpty());
		return;
	}

	TArray<FDynamicMeshVertex> OutVertices;
	if (BuildStringMesh(OutVertices, IndexBuffer.Indices))
	{
		UE_LOG(LogTemp, Log, TEXT("FScanlineTextRenderSceneProxy::CreateRenderThreadResources - Created %d vertices, %d indices"), 
			OutVertices.Num(), IndexBuffer.Indices.Num());

#if RHI_ENABLE_RESOURCE_INFO
		FName Name = FName(TEXT("FScanlineTextRenderSceneProxy ") + GetOwnerName().ToString());
		VertexBuffers.SetOwnerName(Name);
		IndexBuffer.SetOwnerName(Name);
#endif
		VertexBuffers.InitFromDynamicVertex(RHICmdList, &VertexFactory, OutVertices, 4);
		IndexBuffer.InitResource(RHICmdList);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FScanlineTextRenderSceneProxy::CreateRenderThreadResources - BuildStringMesh returned false"));
	}
}

void FScanlineTextRenderSceneProxy::ReleaseRenderThreadResources()
{
	VertexBuffers.PositionVertexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffers.ColorVertexBuffer.ReleaseResource();
	IndexBuffer.ReleaseResource();
	VertexFactory.ReleaseResource();
}

void FScanlineTextRenderSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ScanlineTextRenderSceneProxy_GetDynamicMeshElements);

	// Vertex factory will not be initialized when the text string is empty or font is invalid
	if (!VertexFactory.IsInitialized())
	{
		return;
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			// Draw each text batch
			for (const FTextBatch& TextBatch : TextBatches)
			{
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				Mesh.VertexFactory = &VertexFactory;
				BatchElement.FirstIndex = TextBatch.IndexBufferOffset;
				BatchElement.NumPrimitives = TextBatch.IndexBufferCount / 3;
				BatchElement.MinVertexIndex = TextBatch.VertexBufferOffset;
				BatchElement.MaxVertexIndex = TextBatch.VertexBufferOffset + TextBatch.VertexBufferCount - 1;
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.bDisableBackfaceCulling = false;
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = !bAlwaysRenderAsText;
				Mesh.CastShadow = false;
				Mesh.bUseWireframeSelectionColoring = IsSelected();

				Mesh.MaterialRenderProxy = TextBatch.Material->GetRenderProxy();

				Collector.AddMesh(ViewIndex, Mesh);
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			RenderBounds(Collector.GetPDI(ViewIndex), View->Family->EngineShowFlags, GetBounds(), IsSelected());
#endif
		}
	}
}

void FScanlineTextRenderSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	// Vertex factory will not been initialized when the font is invalid or the text string is empty.
	if (VertexFactory.IsInitialized())
	{
		PDI->ReserveMemoryForMeshes(TextBatches.Num());

		for (const FTextBatch& TextBatch : TextBatches)
		{
			// Draw the mesh.
			FMeshBatch Mesh;
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			BatchElement.IndexBuffer = &IndexBuffer;
			Mesh.VertexFactory = &VertexFactory;
			Mesh.MaterialRenderProxy = TextBatch.Material->GetRenderProxy();
			BatchElement.FirstIndex = TextBatch.IndexBufferOffset;
			BatchElement.NumPrimitives = TextBatch.IndexBufferCount / 3;
			BatchElement.MinVertexIndex = TextBatch.VertexBufferOffset;
			BatchElement.MaxVertexIndex = TextBatch.VertexBufferOffset + TextBatch.VertexBufferCount - 1;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.bDisableBackfaceCulling = false;
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.LODIndex = 0;
			PDI->DrawMesh(Mesh, 1.0f);
		}
	}
}

FPrimitiveViewRelevance FScanlineTextRenderSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;

	// Use dynamic relevance when in rich views or when selected/hovered, otherwise use static
	if (IsRichView(*View->Family)
		|| View->Family->EngineShowFlags.Bounds
		|| View->Family->EngineShowFlags.Collision
		|| IsSelected()
		|| IsHovered())
	{
		Result.bDynamicRelevance = true;
	}
	else
	{
		Result.bStaticRelevance = true;
	}

	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

bool FScanlineTextRenderSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}

uint32 FScanlineTextRenderSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

uint32 FScanlineTextRenderSceneProxy::GetAllocatedSize() const
{
	return FPrimitiveSceneProxy::GetAllocatedSize();
}

bool FScanlineTextRenderSceneProxy::BuildStringMesh(TArray<FDynamicMeshVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	TextBatches.Reset();

	if(!FontFace || Text.IsEmpty())
	{
		return false;
	}

	const FVector2D EmToWorldScale = FontFace->CalculateEmToWorldScale(XScale, YScale, WorldSize);

	float FirstLineHeight = -1.f; // Only kept around for legacy positioning support
	float StartY = 0.f;

	const float CharIncrement = HorizSpacingAdjust * EmToWorldScale.X;

	float LineX = 0.f;

	int32 PageIndex = INDEX_NONE;

	int32 FirstVertexIndexInTextBatch = 0;
	int32 FirstIndiceIndexInTextBatch = 0;
	auto FinishTextBatch = [&]()
	{
		if (OutVertices.Num() > FirstVertexIndexInTextBatch && OutIndices.Num() > FirstIndiceIndexInTextBatch)
		{
			FTextBatch& TextBatch = TextBatches[TextBatches.AddUninitialized()];

			TextBatch.IndexBufferOffset = FirstIndiceIndexInTextBatch;
			TextBatch.IndexBufferCount = OutIndices.Num() - FirstIndiceIndexInTextBatch;

			TextBatch.VertexBufferOffset = FirstVertexIndexInTextBatch;
			TextBatch.VertexBufferCount = OutVertices.Num() - FirstVertexIndexInTextBatch;

			TextBatch.Material = TextMaterial;
		}

		FirstVertexIndexInTextBatch = OutVertices.Num();
		FirstIndiceIndexInTextBatch = OutIndices.Num();
	};

	// TArray uses int32 for size so it cannot store more than MAX_int32 elements.
	const uint64 MaxVerts = MAX_int32 - OutVertices.Num();
	const uint64 MaxIndices = MAX_int32 - OutIndices.Num();
	const uint64 MaxQuads = FMath::Min(MaxVerts / 4, MaxIndices / 6);

	// We have one quad per glyph, so presize the index and vertex buffer.
	const FString& Str = Text.ToString();
	const uint64 NumQuads = FMath::Min((uint64)Str.Len(), MaxQuads);
	OutVertices.Reserve(OutVertices.Num() + NumQuads * 4);
	OutIndices.Reserve(OutIndices.Num() + NumQuads * 6);

	FTextIterator It(*Str);
	for (uint64 QuadIdx = 0; It.NextLine() && QuadIdx < NumQuads;)
	{
		const FVector2D LineSize = ComputeTextSize(It, FontFace, EmToWorldScale.X, EmToWorldScale.Y, HorizSpacingAdjust, VertSpacingAdjust);
		const float StartX = ComputeHorizontalAlignmentOffset(LineSize, HorizontalAlignment);

		if (FirstLineHeight < 0.f)
		{
			FirstLineHeight = LineSize.Y;
		}

		LineX = 0.f;

		TCHAR Ch = 0;
		while (It.NextCharacterInLine(Ch) && QuadIdx < NumQuads)
		{
			FScanlineFontGlyph Glyph;
			if (!FontFace->GetGlyph(Ch, Glyph))
			{
				continue;
			}

			// Get the UV coordinate for this glyph in the glyph index texture
			// Some codepoints wont have a glyph in the font texture, so we need to skip them
			FVector2D GlyphIndexUV(0.0, 0.0);
			const bool HasGlyph = FontFace->GetUVForCodepoint((int32)Ch, GlyphIndexUV);

			// Build mesh for this glyph
			FVector2D Position = FVector2D(LineX, 0.f) + FVector2D(StartX, StartY);

			// If we don't have a glyph don't build a mesh obviously, 
			// but we still want to advance the cursor so that the next character is in the right place.
			if (HasGlyph)
			{
				BuildGlyphMesh(&Glyph, Position, GlyphIndexUV, EmToWorldScale, FVector2D(FontFace->Padding, FontFace->Padding), OutVertices, OutIndices);
			}

			// Advance X position
			const float GlyphWidth = Glyph.AdvanceWidth * EmToWorldScale.X;
			LineX += GlyphWidth;

			TCHAR NextChar = 0;
			if (It.Peek(NextChar) && !FChar::IsWhitespace(NextChar))
			{
				LineX += CharIncrement;
			}

			QuadIdx++;
		}

		// Move Y position down to next line. If the current line is empty, move by max char height in font
		StartY += LineSize.Y > 0.f ? LineSize.Y : FontFace->GetLineHeight();
	}

	FinishTextBatch();

	//// Avoid initializing RHI resources when no vertices are generated.
	return (OutVertices.Num() > 0);
}

void FScanlineTextRenderSceneProxy::BuildGlyphMesh(
	const FScanlineFontGlyph* Glyph,
	const FVector2D& Position,
	const FVector2D& GlyphIndexUV,
	const FVector2D& Scale,
	const FVector2D& Padding,
	TArray<FDynamicMeshVertex>& OutVertices,
	TArray<uint32>& OutIndices)
{
	if (!Glyph)
	{
		return;
	}

	// Build a simple quad for this glyph using its bounding box
	FBox2D BBox = Glyph->BoundingBox;

	BBox = BBox.ExpandBy(Padding);

	// Skip glyphs with invalid bounding boxes
	if (!BBox.bIsValid || BBox.GetSize().X <= 0.0f || BBox.GetSize().Y <= 0.0f)
	{
		return;
	}

	check(BBox.Min.X <= BBox.Max.X && BBox.Min.Y <= BBox.Max.Y);

	// Position the glyph quad
	// Position.Y is at the baseline
	// In FreeType coordinates: yMin is bottom (can be negative for descenders), yMax is top (positive for ascenders)
	// In Unreal rendering: we flip Y axis, so top of glyph should be at Position.Y - BBox.Max.Y
	const float X = Position.X + BBox.Min.X * Scale.X;
	const float Y = Position.Y - (BBox.Max.Y - (FontFace->Ascender)) * Scale.Y;
	const float SizeX = BBox.GetSize().X * Scale.X;
	const float SizeY = BBox.GetSize().Y * Scale.Y;

	const float Left = X;
	const float Top = Y;
	const float Right = X + SizeX;
	const float Bottom = Y + SizeY;

	const FVector4f V0(0.f, -Left, -Top, 0.f);
	const FVector4f V1(0.f, -Right, -Top, 0.f);
	const FVector4f V2(0.f, -Left, -Bottom, 0.f);
	const FVector4f V3(0.f, -Right, -Bottom, 0.f);

	const FVector3f TangentX(0.f, -1.f, 0.f);
	const FVector3f TangentZ(1.f, 0.f, 0.f);

	// UV0: texture coordinates for the glyph quad (0,0) at top-left, (1,1) at bottom-right
	// UV1: glyph index texture lookup coordinates (constant across the quad)
	const int32 V00 = OutVertices.Add(FDynamicMeshVertex(V0, TangentX, TangentZ, FVector2f(0.f, 1.f), TextRenderColor));
	const int32 V10 = OutVertices.Add(FDynamicMeshVertex(V1, TangentX, TangentZ, FVector2f(1.f, 1.f), TextRenderColor));
	const int32 V01 = OutVertices.Add(FDynamicMeshVertex(V2, TangentX, TangentZ, FVector2f(0.f, 0.f), TextRenderColor));
	const int32 V11 = OutVertices.Add(FDynamicMeshVertex(V3, TangentX, TangentZ, FVector2f(1.f, 0.f), TextRenderColor));

	// Set UV1 to the glyph index texture coordinates (same for all vertices of this quad)
	OutVertices[V00].TextureCoordinate[1] = FVector2f(GlyphIndexUV.X, GlyphIndexUV.Y);
	OutVertices[V10].TextureCoordinate[1] = FVector2f(GlyphIndexUV.X, GlyphIndexUV.Y);
	OutVertices[V01].TextureCoordinate[1] = FVector2f(GlyphIndexUV.X, GlyphIndexUV.Y);
	OutVertices[V11].TextureCoordinate[1] = FVector2f(GlyphIndexUV.X, GlyphIndexUV.Y);

	// Set UV2 to the offset
	const FVector2f Min = FVector2f(BBox.Min);
	const FVector2f Max = FVector2f(BBox.Max);

	OutVertices[V01].TextureCoordinate[2] = Min;
	OutVertices[V11].TextureCoordinate[2] = { Max.X, Min.Y };
	OutVertices[V00].TextureCoordinate[2] = { Min.X, Max.Y };
	OutVertices[V10].TextureCoordinate[2] = Max;

	OutVertices[V01].TextureCoordinate[3] = FVector2f(Glyph->CodePoint, Glyph->CodePoint);
	OutVertices[V11].TextureCoordinate[3] = FVector2f(Glyph->CodePoint, Glyph->CodePoint);
	OutVertices[V00].TextureCoordinate[3] = FVector2f(Glyph->CodePoint, Glyph->CodePoint);
	OutVertices[V10].TextureCoordinate[3] = FVector2f(Glyph->CodePoint, Glyph->CodePoint);

	// Two triangles forming a quad
	OutIndices.Add(V00);
	OutIndices.Add(V11);
	OutIndices.Add(V10);

	OutIndices.Add(V00);
	OutIndices.Add(V01);
	OutIndices.Add(V11);
}

UE_ENABLE_OPTIMIZATION
