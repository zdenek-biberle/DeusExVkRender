#pragma once

#include "Precomp.h"

class UVkRender : public URenderBase {
	DECLARE_CLASS(UVkRender, URenderBase, CLASS_Config)

	UVkRender();
	void StaticConstructor();

	// Init/exit functions.
	virtual void Init(UEngine* InEngine);
	virtual UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar = *GLog);

	// Prerender/postrender functions.
	virtual void PreRender(FSceneNode* Frame);
	virtual void PostRender(FSceneNode* Frame);

	// Scene frame management.
	virtual FSceneNode* CreateMasterFrame(UViewport* Viewport, FVector Location, FRotator Rotation, FScreenBounds* Bounds);
	virtual FSceneNode* CreateChildFrame(FSceneNode* Parent, FSpanBuffer* Span, ULevel* Level, INT iSurf, INT iZone, FLOAT Mirror, const FPlane& NearClip, const FCoords& Coords, FScreenBounds* Bounds);
	virtual void FinishMasterFrame();

	// Major rendering functions.
	virtual void DrawWorld(FSceneNode* Frame);
	virtual void DrawActor(FSceneNode* Frame, AActor* Actor);

	// Other functions.
	virtual UBOOL Project(FSceneNode* Frame, const FVector& V, FLOAT& ScreenX, FLOAT& ScreenY, FLOAT* Scale);
	virtual UBOOL Deproject(FSceneNode* Frame, INT ScreenX, INT ScreenY, FVector& V);
	virtual UBOOL BoundVisible(FSceneNode* Frame, FBox* Bound, FSpanBuffer* SpanBuffer, FScreenBounds& Results);
	virtual void GetVisibleSurfs(UViewport* Viewport, TArray<INT>& iSurfs);
	virtual void GlobalLighting(UBOOL Realtime, AActor* Owner, FLOAT& Brightness, FPlane& Color);
	virtual void Precache(UViewport* Viewport);

	// High level primitive drawing.
	virtual void DrawCircle(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector& Location, FLOAT Radius);
	virtual void DrawBox(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector Min, FVector Max);

private:
	URenderBase* impl;
	int someInt;
};