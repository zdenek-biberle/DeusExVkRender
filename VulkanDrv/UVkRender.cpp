#include "Precomp.h"
#include "UVkRender.h"
#include "DeusEx/Render/Src/RenderPrivate.h"
#include "UVulkanRenderDevice.h"

IMPLEMENT_CLASS(UVkRender);

#define UObjectFmt "%s@%x"
#define UObjectVal(x) ((x) ? (x)->GetFullName() : TEXT("null")), (x)

#define FPlaneFmt "FPlane{ x=%f, y=%f, z=%f, w=%f }"
#define FPlaneVal(x) (x).X, (x).Y, (x).Z, (x).W

#define FSceneNodeFmt "FSceneNode{ Viewport=" UObjectFmt ", Level=" UObjectFmt ", Parent=%x, Sibling=%x, Child=%x, iSurf=%d, ZoneNumber=%d, Recursion=%d, Mirror=%f, NearClip=" FPlaneFmt " }"
#define FSceneNodeVal(x) UObjectVal((x).Viewport), UObjectVal((x).Level), (x).Parent, (x).Sibling, (x).Child, (x).iSurf, (x).ZoneNumber, (x).Recursion, (x).Mirror, FPlaneVal((x).NearClip)

#define LogImpl() debugf(TEXT("Impl=" UObjectFmt), UObjectVal(impl))


UVkRender::UVkRender() {
}

void UVkRender::StaticConstructor() {
	URenderBase::StaticConstructor();

	new(GetClass(), TEXT("Impl"), RF_Public) UObjectProperty(CPP_PROPERTY(impl), TEXT("Display"), CPF_Config, URenderBase::StaticClass());
	new(GetClass(), TEXT("Foo"), RF_Public) UIntProperty(CPP_PROPERTY(someInt), TEXT("Display"), CPF_Config);

}

void UVkRender::Init(UEngine* InEngine) {
	auto implClass = LoadClass<URenderBase>(nullptr, TEXT("ini:Engine.Engine.RenderImpl"), nullptr, LOAD_Throw, nullptr);
	impl = ConstructObject<URenderBase>(implClass, this);
	debugf(TEXT("UVkRender::Init(" UObjectFmt ") { Impl=" UObjectFmt " }"), UObjectVal(InEngine), UObjectVal(impl));
	LogImpl();
	Engine = InEngine;
	impl->Init(InEngine);
	//GRender = reinterpret_cast<URender*>(this);
}

UBOOL UVkRender::Exec(const TCHAR* Cmd, FOutputDevice& A) {
	//debugf(TEXT("UVkRender::Exec(%s)"), Cmd);
	//LogImpl();
	return impl->Exec(Cmd, A);
}

// Prerender/postrender functions.
void UVkRender::PreRender(FSceneNode* Frame) {
	//debugf(TEXT("-> UVkRender::PreRender(" FSceneNodeFmt ")"), FSceneNodeVal(*Frame));
	//LogImpl();
	impl->PreRender(Frame);
	//debugf(TEXT("<- UVkRender::PreRender(" FSceneNodeFmt ")"), FSceneNodeVal(*Frame));
}

void UVkRender::PostRender(FSceneNode* Frame) {
	//debugf(TEXT("-> UVkRender::PostRender(" FSceneNodeFmt ")"), FSceneNodeVal(*Frame));
	//LogImpl();
	impl->PostRender(Frame);
	//debugf(TEXT("<- UVkRender::PostRender(" FSceneNodeFmt ")"), FSceneNodeVal(*Frame));
}

// Scene frame management.
FSceneNode* UVkRender::CreateMasterFrame(UViewport* Viewport, FVector Location, FRotator Rotation, FScreenBounds* Bounds) {
	//debugf(TEXT("-> UVkRender::CreateMasterFrame"));
	//LogImpl();
	auto result = impl->CreateMasterFrame(Viewport, Location, Rotation, Bounds);
	//debugf(TEXT("<- UVkRender::CreateMasterFrame: " FSceneNodeFmt), FSceneNodeVal(*result));
	return result;
}

FSceneNode* UVkRender::CreateChildFrame(FSceneNode* Parent, FSpanBuffer* Span, ULevel* Level, INT iSurf, INT iZone, FLOAT Mirror, const FPlane& NearClip, const FCoords& Coords, FScreenBounds* Bounds) {
	//debugf(TEXT("-> UVkRender::CreateChildFrame"));
	//LogImpl();
	auto result = impl->CreateChildFrame(Parent, Span, Level, iSurf, iZone, Mirror, NearClip, Coords, Bounds);
	//debugf(TEXT("<- UVkRender::CreateChildFrame: " FSceneNodeFmt), FSceneNodeVal(*result));
	return nullptr;
}

void UVkRender::FinishMasterFrame() {
	//debugf(TEXT("-> UVkRender::FinishMasterFrame"));
	//LogImpl();
	impl->FinishMasterFrame();
	//debugf(TEXT("<- UVkRender::FinishMasterFrame"));
}

int countVerts(UModel* model, INT iNode) {
	if (iNode == INDEX_NONE) return 0;
	auto& node = model->Nodes(iNode);
	return node.NumVertices + countVerts(model, node.iFront) + countVerts(model, node.iBack);
}

void dumpBsp(UModel* model, INT iNode, int indent) {
	if (iNode == INDEX_NONE) return;
	auto &node = model->Nodes(iNode);
	debugf(TEXT("%*s- %d: %d verts, %d total verts, %d/%d iLeaf"), indent, TEXT(""), iNode, node.NumVertices, countVerts(model, iNode), node.iLeaf[0], node.iLeaf[1]);
	dumpBsp(model, node.iFront, indent + 2);
	dumpBsp(model, node.iBack, indent + 2);
}

void dumpTexture(UTexture* texture, UTexture* stop, int indent) {
	if (!texture) return;
	if (texture == stop) {
		debugf(TEXT("%*s(stop)"), indent, TEXT(""));
	}
	if (stop == nullptr) {
		stop = texture;
	}
	debugf(TEXT("%*s- Format: %d"), indent, TEXT(""), texture->Format);
	debugf(TEXT("%*s- Palette: " UObjectFmt), indent, TEXT(""), UObjectVal(texture->Palette));
	debugf(TEXT("%*s- USize/VSize: %d/%d"), indent, TEXT(""), texture->USize, texture->VSize);
	debugf(TEXT("%*s- UClamp/VClamp: %d/%d"), indent, TEXT(""), texture->UClamp, texture->VClamp);
	debugf(TEXT("%*s- Diffuse: %f"), indent, TEXT(""), texture->Diffuse);
	debugf(TEXT("%*s- Specular: %f"), indent, TEXT(""), texture->Specular);
	debugf(TEXT("%*s- Alpha: %f"), indent, TEXT(""), texture->Alpha);
	debugf(TEXT("%*s- Scale: %f"), indent, TEXT(""), texture->Scale);
	debugf(TEXT("%*s- MipMult: %f"), indent, TEXT(""), texture->MipMult);
	debugf(TEXT("%*s- Mips: %d"), indent, TEXT(""), texture->Mips.Num());	
	debugf(TEXT("%*s- CompMips: %d"), indent, TEXT(""), texture->CompMips.Num());
	debugf(TEXT("%*s- CompFormat: %d"), indent, TEXT(""), texture->CompFormat);
	if (texture->AnimNext) {
		debugf(TEXT("%*s- AnimNext: " UObjectFmt), indent, TEXT(""), UObjectVal(texture->AnimNext));
		//dumpTexture(texture->AnimNext, stop, indent + 2);
	}
	if (texture->AnimCur) {
		debugf(TEXT("%*s- AnimCur: " UObjectFmt), indent, TEXT(""), UObjectVal(texture->AnimCur));
		//dumpTexture(texture->AnimCur, stop, indent + 2);
	}
	if (texture->BumpMap) {
		debugf(TEXT("%*s- BumpMap: " UObjectFmt), indent, TEXT(""), UObjectVal(texture->BumpMap));
		//dumpTexture(texture->BumpMap, stop, indent + 2);
	}
	if (texture->DetailTexture) {
		debugf(TEXT("%*s- DetailTexture: " UObjectFmt), indent, TEXT(""), UObjectVal(texture->DetailTexture));
		//dumpTexture(texture->DetailTexture, stop, indent + 2);
	}
	if (texture->MacroTexture) {
		debugf(TEXT("%*s- MacroTexture: " UObjectFmt), indent, TEXT(""), UObjectVal(texture->MacroTexture));
		//dumpTexture(texture->MacroTexture, stop, indent + 2);
	}
}

// Major rendering functions.
void UVkRender::DrawWorld(FSceneNode* Frame) {
	static_cast<UVulkanRenderDevice*>(Frame->Viewport->RenDev)->DrawWorld(Frame);

	auto model = Frame->Level->Model;

	if (!model) return;

	static UModel* lastModel = nullptr;
	if (lastModel != model) {
		lastModel = model;
		// dump it
		debugf(TEXT("Level: " UObjectFmt), UObjectVal(Frame->Level));
		debugf(TEXT("Model: " UObjectFmt), UObjectVal(model));
		debugf(TEXT("- Polys: " UObjectFmt), UObjectVal(model->Polys));
		debugf(TEXT("- Nodes: %d"), model->Nodes.Num());
		debugf(TEXT("- Verts: %d"), model->Verts.Num());
		debugf(TEXT("- Vectors: %d"), model->Vectors.Num());
		debugf(TEXT("- Points: %d"), model->Points.Num());
		debugf(TEXT("- Surfs: %d"), model->Surfs.Num());
		debugf(TEXT("- LightMap: %d"), model->LightMap.Num());
		debugf(TEXT("- LightBits: %d"), model->LightBits.Num());
		debugf(TEXT("- Bounds: %d"), model->Bounds.Num());
		debugf(TEXT("- LeafHulls: %d"), model->LeafHulls.Num());
		debugf(TEXT("- Leaves: %d"), model->Leaves.Num());
		debugf(TEXT("- Lights: %d"), model->Lights.Num());
		//debugf(TEXT("- Tree:"));
		//dumpBsp(model, 0, 2);
		//std::set<UTexture*> textures;
		//for (int i = 0; i < model->Surfs.Num(); i++) {
		//	auto& surf = model->Surfs(i);
		//	if (surf.Texture) textures.insert(surf.Texture);
		//}
		//debugf(TEXT("- Textures: %d"), textures.size());
		//for (auto tex : textures) {
		//	debugf(TEXT("  - " UObjectFmt), UObjectVal(tex));
		//	dumpTexture(tex, nullptr, 4);
		//}
	}

	//for (INT i = 0; i < model->Nodes.Num(); i++) {
	//	auto& node = model->Nodes(i);
	//	auto& surf = model->Surfs(node.iSurf);

	//	FTextureInfo texInfo;
	//	texInfo.Texture = surf.Texture;
	//	texInfo.MaxColor = nullptr;
	//	texInfo.Palette = nullptr;
	//	if (surf.Texture)
	//		surf.Texture->Lock(texInfo, 0, 0, Frame->Viewport->RenDev);

	//	std::vector<FTransTexture> points(node.NumVertices);
	//	std::vector<FTransTexture*> ppoints(node.NumVertices);

	//	auto textureU = model->Vectors(surf.vTextureU);
	//	auto textureV = model->Vectors(surf.vTextureV);
	//	auto basePoint = model->Points(surf.pBase);
	//	float uOffset = (basePoint | textureU) + texInfo.Pan.X;
	//	float vOffset = (basePoint | textureV) + texInfo.Pan.Y;

	//	if (texInfo.Pan.X != 0 || texInfo.Pan.Y != 0 || texInfo.Pan.Z != 0) {
	//		debugf(TEXT("weird %f %f %f"), texInfo.Pan.X, texInfo.Pan.Y, texInfo.Pan.Z);
	//	}

	//	for (INT j = 0; j < node.NumVertices; j++) {
	//		auto& vert = model->Verts(node.iVertPool + j);
	//		auto position = model->Points(vert.pVertex);

	//		FTransTexture point;
	//		point.Point = position.TransformPointBy(Frame->Coords);
	//		point.Flags = 0;
	//		point.Light = FPlane(1, 1, 1, 1);
	//		point.U = (position | textureU) - uOffset;
	//		point.V = (position | textureV) - vOffset;
	//		points[j] = point;
	//		ppoints[j] = &points[j];
	//	}

	//	Frame->Viewport->RenDev->DrawGouraudPolygon(Frame, texInfo, ppoints.data(), node.NumVertices, 0, nullptr);
	//	if (surf.Texture)
	//		surf.Texture->Unlock(texInfo);
	//}
	//debugf(TEXT("-> UVkRender::DrawWorld(" FSceneNodeFmt ")"), FSceneNodeVal(*Frame));
	/*if (Frame->Level) {
		auto level = Frame->Level;
		debugf(TEXT("    Level: " UObjectFmt), UObjectVal(level));
		for (int i = 0; i < level->Actors.Num(); i++)
			debugf(TEXT("    - Actor: " UObjectFmt), UObjectVal(level->Actors(i)));
		debugf(TEXT("    - Reach specs: %d"), Frame->Level->ReachSpecs.Num());
		if (level->Model) {
			auto model = level->Model;
			debugf(TEXT("    - Model: " UObjectFmt), UObjectVal(model));
			debugf(TEXT("      - Polys: " UObjectFmt), UObjectVal(model->Polys));
			debugf(TEXT("      - Nodes: %d"), model->Nodes.Num());
			debugf(TEXT("      - Verts: %d"), model->Verts.Num());
			debugf(TEXT("      - Vectors: %d"), model->Vectors.Num());
			debugf(TEXT("      - Points: %d"), model->Points.Num());
			debugf(TEXT("      - Surfs: %d"), model->Surfs.Num());
			debugf(TEXT("      - LightMap: %d"), model->LightMap.Num());
			debugf(TEXT("      - LightBits: %d"), model->LightBits.Num());
			debugf(TEXT("      - Bounds: %d"), model->Bounds.Num());
			debugf(TEXT("      - LeafHulls: %d"), model->LeafHulls.Num());
			debugf(TEXT("      - Leaves: %d"), model->Leaves.Num());
			debugf(TEXT("      - Lights: %d"), model->Lights.Num());

		}
		else {
			debugf(TEXT("    - No model :("));
		}

	}
	else {
		debugf(TEXT("    No level :("));
	}

	debugf(TEXT("<- UVkRender::DrawWorld(" FSceneNodeFmt ")"), FSceneNodeVal(*Frame));*/
}

void UVkRender::DrawActor(FSceneNode* Frame, AActor* Actor) {
	debugf(TEXT("UVkRender::DrawActor"));
}

// Other functions.
UBOOL UVkRender::Project(FSceneNode* Frame, const FVector& V, FLOAT& ScreenX, FLOAT& ScreenY, FLOAT* Scale) {
	debugf(TEXT("UVkRender::Project"));
	return 1;
}

UBOOL UVkRender::Deproject(FSceneNode* Frame, INT ScreenX, INT ScreenY, FVector& V) {
	debugf(TEXT("UVkRender::Deproject"));
	return 1;
}

UBOOL UVkRender::BoundVisible(FSceneNode* Frame, FBox* Bound, FSpanBuffer* SpanBuffer, FScreenBounds& Results) {
	debugf(TEXT("UVkRender::BoundVisisble"));
	return 1;
}

void UVkRender::GetVisibleSurfs(UViewport* Viewport, TArray<INT>& iSurfs) {
	debugf(TEXT("UVkRender::GetVisibleSurfs"));
}

void UVkRender::GlobalLighting(UBOOL Realtime, AActor* Owner, FLOAT& Brightness, FPlane& Color) {
	//debugf(TEXT("UVkRender::GlobalLighting"));
}

void UVkRender::Precache(UViewport* Viewport) {
	debugf(TEXT("UVkRender::Precache"));
}


// High level primitive drawing.
void UVkRender::DrawCircle(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector& Location, FLOAT Radius) {
	debugf(TEXT("UVkRender::DrawCircle"));
}

void UVkRender::DrawBox(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector Min, FVector Max) {
	debugf(TEXT("UVkRender::DrawBox"));
}