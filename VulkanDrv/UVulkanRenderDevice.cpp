
#include "Precomp.h"
#include "UVulkanRenderDevice.h"
#include "CachedTexture.h"
#include "UTF16.h"

IMPLEMENT_CLASS(UVulkanRenderDevice);

UVulkanRenderDevice::UVulkanRenderDevice()
{
}

void UVulkanRenderDevice::StaticConstructor()
{
	guard(UVulkanRenderDevice::StaticConstructor);

	SpanBased = 0;
	FullscreenOnly = 0;
	SupportsFogMaps = 1;
	SupportsDistanceFog = 0;
	SupportsTC = 1;
	SupportsLazyTextures = 0;
	PrefersDeferredLoad = 0;
	UseVSync = 1;
	AntialiasMode = 0;
	UsePrecache = 1;
	Coronas = 1;
	ShinySurfaces = 1;
	DetailTextures = 1;
	HighDetailActors = 1;
	VolumetricLighting = 1;

#if defined(OLDUNREAL469SDK)
	UseLightmapAtlas = 0; // Note: do not turn this on. It does not work and generates broken fogmaps.
	SupportsUpdateTextureRect = 1;
	MaxTextureSize = 4096;
	NeedsMaskedFonts = 0;
#endif

	GammaMode = 0;
	GammaOffset = 0.0f;
	GammaOffsetRed = 0.0f;
	GammaOffsetGreen = 0.0f;
	GammaOffsetBlue = 0.0f;

	LinearBrightness = 128; // 0.0f;
	Contrast = 128; // 1.0f;
	Saturation = 255; // 1.0f;
	GrayFormula = 1;

	Hdr = 0;
	OccludeLines = 0;
	Bloom = 0;
	BloomAmount = 128;

	LODBias = -0.5f;
	LightMode = 0;

	VkDeviceIndex = 0;
	VkDebug = 0;
	VkExclusiveFullscreen = 0;

#if defined(OLDUNREAL469SDK)
	new(GetClass(), TEXT("UseLightmapAtlas"), RF_Public) UBoolProperty(CPP_PROPERTY(UseLightmapAtlas), TEXT("Display"), CPF_Config);
#endif

	new(GetClass(), TEXT("UseVSync"), RF_Public) UBoolProperty(CPP_PROPERTY(UseVSync), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("UsePrecache"), RF_Public) UBoolProperty(CPP_PROPERTY(UsePrecache), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffset"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffset), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffsetRed"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffsetRed), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffsetGreen"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffsetGreen), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffsetBlue"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffsetBlue), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("LinearBrightness"), RF_Public) UByteProperty(CPP_PROPERTY(LinearBrightness), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Contrast"), RF_Public) UByteProperty(CPP_PROPERTY(Contrast), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Saturation"), RF_Public) UByteProperty(CPP_PROPERTY(Saturation), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GrayFormula"), RF_Public) UIntProperty(CPP_PROPERTY(GrayFormula), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Hdr"), RF_Public) UBoolProperty(CPP_PROPERTY(Hdr), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("OccludeLines"), RF_Public) UBoolProperty(CPP_PROPERTY(OccludeLines), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Bloom"), RF_Public) UBoolProperty(CPP_PROPERTY(Bloom), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("BloomAmount"), RF_Public) UByteProperty(CPP_PROPERTY(BloomAmount), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("LODBias"), RF_Public) UFloatProperty(CPP_PROPERTY(LODBias), TEXT("Display"), CPF_Config);

	UEnum* AntialiasModes = new(GetClass(), TEXT("AntialiasModes"))UEnum(nullptr);
	new(AntialiasModes->Names)FName(TEXT("Off"));
	new(AntialiasModes->Names)FName(TEXT("MSAA_2x"));
	new(AntialiasModes->Names)FName(TEXT("MSAA_4x"));
	new(GetClass(), TEXT("AntialiasMode"), RF_Public) UByteProperty(CPP_PROPERTY(AntialiasMode), TEXT("Display"), CPF_Config, AntialiasModes);

	UEnum* GammaModes = new(GetClass(), TEXT("GammaModes"))UEnum(nullptr);
	new(GammaModes->Names)FName(TEXT("D3D9"));
	new(GammaModes->Names)FName(TEXT("XOpenGL"));
	new(GetClass(), TEXT("GammaMode"), RF_Public) UByteProperty(CPP_PROPERTY(GammaMode), TEXT("Display"), CPF_Config, GammaModes);

	UEnum* LightModes = new(GetClass(), TEXT("LightModes"))UEnum(nullptr);
	new(LightModes->Names)FName(TEXT("Normal"));
	new(LightModes->Names)FName(TEXT("OneXBlending"));
	new(LightModes->Names)FName(TEXT("BrighterActors"));
	new(GetClass(), TEXT("LightMode"), RF_Public) UByteProperty(CPP_PROPERTY(LightMode), TEXT("Display"), CPF_Config, LightModes);

	new(GetClass(), TEXT("VkDeviceIndex"), RF_Public) UIntProperty(CPP_PROPERTY(VkDeviceIndex), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("VkDebug"), RF_Public) UBoolProperty(CPP_PROPERTY(VkDebug), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("VkExclusiveFullscreen"), RF_Public) UBoolProperty(CPP_PROPERTY(VkExclusiveFullscreen), TEXT("Display"), CPF_Config);

	unguard;
}

void VulkanPrintLog(const char* typestr, const std::string& msg)
{
	debugf(TEXT("[%s] %s"), to_utf16(typestr).c_str(), to_utf16(msg).c_str());
}

void VulkanError(const char* text)
{
	throw std::runtime_error(text);
}

UBOOL UVulkanRenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UVulkanRenderDevice::Init);

	Viewport = InViewport;

	try
	{
		auto instance = VulkanInstanceBuilder()
			.RequireSurfaceExtensions()
			.DebugLayer(VkDebug)
			.Create();

		auto surface = VulkanSurfaceBuilder()
			.Win32Window((HWND)Viewport->GetWindow())
			.Create(instance);

		for (auto& phys : instance->PhysicalDevices) {
			debugf(TEXT("physical device: %s"), to_utf16(phys.Properties.Properties.deviceName).c_str());
		}

		Device = VulkanDeviceBuilder()
			.Surface(surface)
			.OptionalDescriptorIndexing()
			.RequireExtension(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME)
			.SelectDevice(VkDeviceIndex)
			.Create(instance);

		auto supportsBindless =
			Device->EnabledFeatures.DescriptorIndexing.descriptorBindingPartiallyBound &&
			Device->EnabledFeatures.DescriptorIndexing.runtimeDescriptorArray &&
			Device->EnabledFeatures.DescriptorIndexing.shaderSampledImageArrayNonUniformIndexing;

		if (!supportsBindless)
			throw std::runtime_error("Bindless not supported");

		debugf(TEXT("CommandBufferManager"));
		Commands.reset(new CommandBufferManager(this));
		debugf(TEXT("SamplerManager"));
		Samplers.reset(new SamplerManager(this));
		debugf(TEXT("TextureManager"));
		Textures.reset(new TextureManager(this));
		debugf(TEXT("BufferManager"));
		Buffers.reset(new BufferManager(this));
		debugf(TEXT("ShaderManager"));
		Shaders.reset(new ShaderManager(this));
		debugf(TEXT("UploadManager"));
		Uploads.reset(new UploadManager(this));
		debugf(TEXT("DescriptorSetManager"));
		DescriptorSets.reset(new DescriptorSetManager(this));
		debugf(TEXT("RenderPassManager"));
		RenderPasses.reset(new RenderPassManager(this));
		debugf(TEXT("FramebufferManager"));
		Framebuffers.reset(new FramebufferManager(this));

		const auto& props = Device->PhysicalDevice.Properties.Properties;

		FString deviceType;
		switch (props.deviceType)
		{
		case VK_PHYSICAL_DEVICE_TYPE_OTHER: deviceType = TEXT("other"); break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = TEXT("integrated gpu"); break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceType = TEXT("discrete gpu"); break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceType = TEXT("virtual gpu"); break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceType = TEXT("cpu"); break;
		default: deviceType = FString::Printf(TEXT("%d"), (int)props.deviceType); break;
		}

		FString apiVersion, driverVersion;
		apiVersion = FString::Printf(TEXT("%d.%d.%d"), VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));
		driverVersion = FString::Printf(TEXT("%d.%d.%d"), VK_VERSION_MAJOR(props.driverVersion), VK_VERSION_MINOR(props.driverVersion), VK_VERSION_PATCH(props.driverVersion));

		debugf(TEXT("Vulkan device: %s"), to_utf16(props.deviceName).c_str());
		debugf(TEXT("Vulkan device type: %s"), *deviceType);
		debugf(TEXT("Vulkan version: %s (api) %s (driver)"), *apiVersion, *driverVersion);

		if (VkDebug)
		{
			debugf(TEXT("Vulkan extensions:"));
			for (const VkExtensionProperties& p : Device->PhysicalDevice.Extensions)
			{
				debugf(TEXT(" %s"), to_utf16(p.extensionName).c_str());
			}
		}

		const auto& limits = props.limits;
		debugf(TEXT("Max. texture size: %d"), limits.maxImageDimension2D);
		debugf(TEXT("Max. uniform buffer range: %d"), limits.maxUniformBufferRange);
		debugf(TEXT("Min. uniform buffer offset alignment: %llu"), limits.minUniformBufferOffsetAlignment);
	}
	catch (const std::exception& e)
	{
		debugf(TEXT("Could not create vulkan renderer: %s"), to_utf16(e.what()).c_str());
		Exit();
		return 0;
	}

	if (!SetRes(NewX, NewY, NewColorBytes, Fullscreen))
	{
		Exit();
		return 0;
	}

	return 1;
	unguard;
}

UBOOL UVulkanRenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UVulkanRenderDevice::SetRes);

	if (NewX == 0 || NewY == 0)
		return 1;

	if (!Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_OpenGL) : (BLIT_HardwarePaint | BLIT_OpenGL), NewX, NewY, NewColorBytes))
		return 0;

#ifdef USE_HORRIBLE_WIN32_MODE_SWITCHES
	if (Fullscreen)
	{
		DEVMODE devmode = {};
		devmode.dmSize = sizeof(DEVMODE);
		devmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT; // | DM_DISPLAYFREQUENCY
		devmode.dmPelsWidth = NewX;
		devmode.dmPelsHeight = NewY;
		// devmode.dmDisplayFrequency = 360;
		ChangeDisplaySettingsEx(nullptr, &devmode, 0, CDS_FULLSCREEN, 0);
	}
#endif

	SaveConfig();

#if defined(UNREALGOLD)
	Flush();
#else
	Flush(1);
#endif

	return 1;
	unguard;
}

void UVulkanRenderDevice::Exit()
{
	guard(UVulkanRenderDevice::Exit);

	if (Device) vkDeviceWaitIdle(Device->device);

#ifdef USE_HORRIBLE_WIN32_MODE_SWITCHES
	ChangeDisplaySettingsEx(nullptr, nullptr, 0, 0, 0);
#endif

	lastScene.reset();
	Framebuffers.reset();
	RenderPasses.reset();
	DescriptorSets.reset();
	Uploads.reset();
	Shaders.reset();
	Buffers.reset();
	Textures.reset();
	Samplers.reset();
	Commands.reset();

	Device.reset();

	unguard;
}

void UVulkanRenderDevice::SubmitAndWait(bool present, int presentWidth, int presentHeight, bool presentFullscreen)
{
	if (Device) vkDeviceWaitIdle(Device->device);
	//DescriptorSets->UpdateBindlessSet();

	Commands->SubmitCommands(present, presentWidth, presentHeight, presentFullscreen);

	Batch.Pipeline = nullptr;
	//Batch.DescriptorSet = nullptr;
	Batch.SceneIndexStart = 0;
	SceneVertexPos = 0;
	SceneIndexPos = 0;
}

#if defined(UNREALGOLD)

void UVulkanRenderDevice::Flush()
{
	guard(UVulkanRenderDevice::Flush);

	if (IsLocked)
	{
		DrawBatch(Commands->GetDrawCommands());
		RenderPasses->EndScene(Commands->GetDrawCommands());
		SubmitAndWait(false, 0, 0, false);

		ClearTextureCache();

		auto cmdbuffer = Commands->GetDrawCommands();
		RenderPasses->BeginScene(cmdbuffer, 0.0f, 0.0f, 0.0f, 1.0f);

		VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
		cmdbuffer->bindIndexBuffer(Buffers->SceneIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
	}
	else
	{
		ClearTextureCache();
	}

	if (UsePrecache && !GIsEditor)
		PrecacheOnFlip = 1;

	unguard;
}

#else

void UVulkanRenderDevice::Flush(UBOOL AllowPrecache)
{
	guard(UVulkanRenderDevice::Flush);
	debugf(TEXT("Flush!"));

	if (IsLocked)
	{
		DrawBatch(Commands->GetDrawCommands());
		RenderPasses->EndScene(Commands->GetDrawCommands());
		SubmitAndWait(false, 0, 0, false);

		//ClearTextureCache();

		auto cmdbuffer = Commands->GetDrawCommands();
		RenderPasses->BeginScene(cmdbuffer, 0.0f, 0.0f, 0.0f, 1.0f);

		VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
		cmdbuffer->bindIndexBuffer(Buffers->SceneIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
	}
	else
	{
		//ClearTextureCache();
	}

	if (AllowPrecache && UsePrecache && !GIsEditor)
		PrecacheOnFlip = 1;

	unguard;
}

#endif

UBOOL UVulkanRenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	guard(UVulkanRenderDevice::Exec);

	if (ParseCommand(&Cmd, TEXT("DGL")))
	{
		if (ParseCommand(&Cmd, TEXT("BUFFERTRIS")))
		{
			return 1;
		}
		else if (ParseCommand(&Cmd, TEXT("BUILD")))
		{
			return 1;
		}
		else if (ParseCommand(&Cmd, TEXT("AA")))
		{
			return 1;
		}
		return 0;
	}
	else if (ParseCommand(&Cmd, TEXT("GetRes")))
	{
		struct Resolution
		{
			int X;
			int Y;

			// For sorting highest resolution first
			bool operator<(const Resolution& other) const { if (X != other.X) return X > other.X; else return Y > other.Y; }
		};

		std::set<Resolution> resolutions;

#ifdef WIN32
		// Always include what the monitor is currently using
		HDC screenDC = GetDC(0);
		int screenWidth = GetDeviceCaps(screenDC, HORZRES);
		int screenHeight = GetDeviceCaps(screenDC, VERTRES);
		resolutions.insert({ screenWidth, screenHeight });
		ReleaseDC(0, screenDC);

#ifdef USE_HORRIBLE_WIN32_MODE_SWITCHES
		// Get what else is available according to Windows
		DEVMODE devmode = {};
		devmode.dmSize = sizeof(DEVMODE);
		int i = 0;
		while (EnumDisplaySettingsEx(nullptr, i++, &devmode, 0) != 0)
		{
			if ((devmode.dmFields & (DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT)) == (DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT) && devmode.dmBitsPerPel >= 24)
			{
				resolutions.insert({ (int)devmode.dmPelsWidth, (int)devmode.dmPelsHeight });
			}

			devmode = {};
			devmode.dmSize = sizeof(DEVMODE);
		}

		// Add a letterboxed 4:3 mode for widescreen monitors
		resolutions.insert({ (screenHeight * 4 + 2) / 3, screenHeight });

		// Include a few classics from the era
		resolutions.insert({ 800, 600 });
		resolutions.insert({ 1024, 768 });
		resolutions.insert({ 1600, 1200 });
#endif
#else
		resolutions.insert({ 800, 600 });
		resolutions.insert({ 1280, 720 });
		resolutions.insert({ 1024, 768 });
		resolutions.insert({ 1280, 768 });
		resolutions.insert({ 1152, 864 });
		resolutions.insert({ 1280, 900 });
		resolutions.insert({ 1280, 1024 });
		resolutions.insert({ 1400, 1024 });
		resolutions.insert({ 1920, 1080 });
		resolutions.insert({ 2560, 1440 });
		resolutions.insert({ 2560, 1600 });
		resolutions.insert({ 3840, 2160 });
#endif

		FString Str;
		for (const Resolution& resolution : resolutions)
		{
			Str += FString::Printf(TEXT("%ix%i "), (INT)resolution.X, (INT)resolution.Y);
		}
		Ar.Log(*Str.LeftChop(1));
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("GetVkDevices")))
	{
		std::vector<VulkanCompatibleDevice> supportedDevices = VulkanDeviceBuilder()
			.Surface(Device->Surface)
			.RequireExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
			//.OptionalDescriptorIndexing()
			.FindDevices(Device->Instance);

		for (size_t i = 0; i < supportedDevices.size(); i++)
		{
			Ar.Log(FString::Printf(TEXT("#%d - %s\r\n"), (int)i, to_utf16(supportedDevices[i].Device->Properties.Properties.deviceName)));
		}
		return 1;
	}
	else
	{
#if !defined(UNREALGOLD)
		return Super::Exec(Cmd, Ar);
#else
		return 0;
#endif
	}

	unguard;
}

void UVulkanRenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize)
{
	guard(UVulkanRenderDevice::Lock);
	vkDeviceWaitIdle(Device->device);
	//debugf(TEXT("Lock!"));

	//HitData = InHitData;
	//HitSize = InHitSize;

	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	pushconstants.hitIndex = 0;
	//ForceHitIndex = -1;

	try
	{
		// If frame textures no longer match the window or user settings, recreate them along with the swap chain
		if (!Textures->Scene || Textures->Scene->Width != Viewport->SizeX || Textures->Scene->Height != Viewport->SizeY || Textures->Scene->Multisample != GetSettingsMultisample())
		{
			debugf(TEXT("need to recreate frame texture & swap chain, width: %d, height: %d"), Viewport->SizeX, Viewport->SizeY);
			//Framebuffers->DestroySceneFramebuffer();
			debugf(TEXT("Reset scene texture"));
			Textures->Scene.reset();
			debugf(TEXT("Set scene texture"));
			Textures->Scene.reset(new SceneTextures(this, Viewport->SizeX, Viewport->SizeY, GetSettingsMultisample()));
			//debugf(TEXT("CreateRenderPass"));
			//RenderPasses->CreateRenderPass();
			//debugf(TEXT("CreatePipelines"));
			//RenderPasses->CreatePipelines();
			//debugf(TEXT("CreateSceneFramebuffer"));
			//Framebuffers->CreateSceneFramebuffer();
			//debugf(TEXT("UpdateFrameDescriptors"));
			//DescriptorSets->UpdateFrameDescriptors();
			debugf(TEXT("need to recreate frame texture & swap chain DONE"));
		}

		//PipelineBarrier()
			//.AddImage(
			//	Textures->Scene->ColorBuffer.get(),
			//	VK_IMAGE_LAYOUT_UNDEFINED,
			//	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			//	VK_ACCESS_SHADER_READ_BIT,
			//	VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			//	VK_IMAGE_ASPECT_COLOR_BIT)
			//.AddImage(
			//	Textures->Scene->HitBuffer.get(),
			//	VK_IMAGE_LAYOUT_UNDEFINED,
			//	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			//	VK_ACCESS_SHADER_READ_BIT,
			//	VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			//	VK_IMAGE_ASPECT_COLOR_BIT)
			//.Execute(Commands->GetDrawCommands(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		auto cmdbuffer = Commands->GetDrawCommands();
		Commands->AcquirePresentImage(Viewport->SizeX, Viewport->SizeY, Viewport->IsFullscreen());
		RenderPasses->BeginScene(cmdbuffer, ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W);

		VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
		cmdbuffer->bindIndexBuffer(Buffers->SceneIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

		IsLocked = true;
	}
	catch (const std::exception& e)
	{
		// To do: can we report this back to unreal in a better way?
		MessageBoxA(0, e.what(), "Vulkan Error", MB_OK);
		exit(0);
	}

	unguard;
}

void UVulkanRenderDevice::DrawStats(FSceneNode* Frame)
{
	Super::DrawStats(Frame);

#if defined(OLDUNREAL469SDK)
	GRender->ShowStat(CurrentFrame, TEXT("Vulkan: Draw calls: %d, Complex surfaces: %d, Gouraud polygons: %d, Tiles: %d; Uploads: %d, Rect Uploads: %d\r\n"), Stats.DrawCalls, Stats.ComplexSurfaces, Stats.GouraudPolygons, Stats.Tiles, Stats.Uploads, Stats.RectUploads);
#endif

	Stats.DrawCalls = 0;
	Stats.ComplexSurfaces = 0;
	Stats.GouraudPolygons = 0;
	Stats.Tiles = 0;
	Stats.Uploads = 0;
	Stats.RectUploads = 0;
}

void UVulkanRenderDevice::Unlock(UBOOL Blit)
{
	guard(UVulkanRenderDevice::Unlock);

	try
	{
		DrawBatch(Commands->GetDrawCommands());
		RenderPasses->EndScene(Commands->GetDrawCommands());
		SubmitAndWait(Blit ? true : false, Viewport->SizeX, Viewport->SizeY, Viewport->IsFullscreen());
		IsLocked = false;
	}
	catch (std::exception& e)
	{
		static std::wstring err;
		err = to_utf16(e.what());
		appUnwindf(TEXT("%s"), err.c_str());
	}

	//debugf(TEXT("Vulkan: Unlock: Draw calls: %d, Complex surfaces: %d, Gouraud polygons: %d, Tiles: %d; Uploads: %d, Rect Uploads: %d"), Stats.DrawCalls, Stats.ComplexSurfaces, Stats.GouraudPolygons, Stats.Tiles, Stats.Uploads, Stats.RectUploads);

	unguard;
}

#if defined(OLDUNREAL469SDK)

UBOOL UVulkanRenderDevice::SupportsTextureFormat(ETextureFormat Format)
{
	guard(UVulkanRenderDevice::SupportsTextureFormat);

	return Uploads->SupportsTextureFormat(Format) ? TRUE : FALSE;

	unguard;
}

void UVulkanRenderDevice::UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL)
{
	guardSlow(UVulkanRenderDevice::UpdateTextureRect);

	Textures->UpdateTextureRect(&Info, U, V, UL, VL);

	unguardSlow;
}

#endif

void UVulkanRenderDevice::DrawBatch(VulkanCommandBuffer* cmdbuffer)
{
	size_t icount = SceneIndexPos - Batch.SceneIndexStart;
	if (icount > 0)
	{
		auto layout = RenderPasses->Scene.BindlessPipelineLayout.get();
		cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, Batch.Pipeline);
		//cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, Batch.DescriptorSet);
		cmdbuffer->pushConstants(layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants), &pushconstants);
		cmdbuffer->drawIndexed(icount, 1, Batch.SceneIndexStart, 0, 0);
		Batch.SceneIndexStart = SceneIndexPos;
		Stats.DrawCalls++;
	}
}

void UVulkanRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guardSlow(UVulkanRenderDevice::DrawComplexSurface);
	int numPolys = 0;
	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next) {
		numPolys++;
	}

#if defined(UNREALGOLD)
	if (Surface.DetailTexture && Surface.FogMap) detailtex = nullptr;
#else
	//if ((Surface.DetailTexture && Surface.FogMap) || (!DetailTextures)) detailtex = nullptr;
#endif

	float UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	float VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	float UPan = /*tex ? UDot + Surface.Texture->Pan.X :*/ 0.0f;
	float VPan = /*tex ? VDot + Surface.Texture->Pan.Y :*/ 0.0f;
	float UMult = /*tex ? GetUMult(*Surface.Texture) :*/ 0.0f;
	float VMult = /*tex ? GetVMult(*Surface.Texture) :*/ 0.0f;
	float LMUPan = /*lightmap ? UDot + Surface.LightMap->Pan.X - 0.5f * Surface.LightMap->UScale :*/ 0.0f;
	float LMVPan = /*lightmap ? VDot + Surface.LightMap->Pan.Y - 0.5f * Surface.LightMap->VScale :*/ 0.0f;
	float LMUMult = /*lightmap ? GetUMult(*Surface.LightMap) :*/ 0.0f;
	float LMVMult = /*lightmap ? GetVMult(*Surface.LightMap) :*/ 0.0f;
	float MacroUPan = /*macrotex ? UDot + Surface.MacroTexture->Pan.X :*/ 0.0f;
	float MacroVPan = /*macrotex ? VDot + Surface.MacroTexture->Pan.Y :*/ 0.0f;
	float MacroUMult = /*macrotex ? GetUMult(*Surface.MacroTexture) :*/ 0.0f;
	float MacroVMult = /*macrotex ? GetVMult(*Surface.MacroTexture) :*/ 0.0f;
	float DetailUPan = UPan;
	float DetailVPan = VPan;
	float DetailUMult = /*detailtex ? GetUMult(*Surface.DetailTexture) :*/ 0.0f;
	float DetailVMult = /*detailtex ? GetVMult(*Surface.DetailTexture) :*/ 0.0f;

	uint32_t flags = 0;
	//if (lightmap) flags |= 1;
	//if (macrotex) flags |= 2;
	//if (detailtex && !fogmap) flags |= 4;
	//if (fogmap) flags |= 8;

	if (LightMode == 1) flags |= 64;

	//if (fogmap) // if Surface.FogMap exists, use instead of detail texture
	//{
	//	detailtex = fogmap;
	//	DetailUPan = UDot + Surface.FogMap->Pan.X - 0.5f * Surface.FogMap->UScale;
	//	DetailVPan = VDot + Surface.FogMap->Pan.Y - 0.5f * Surface.FogMap->VScale;
	//	DetailUMult = GetUMult(*Surface.FogMap);
	//	DetailVMult = GetVMult(*Surface.FogMap);
	//}

	SetPipeline(RenderPasses->GetPipeline(Surface.PolyFlags));

	vec4 color(1.0f);

	// Draw the surface twice if the editor selected it. Second time highlighted without textures
	int drawcount = (Surface.PolyFlags & PF_Selected) && GIsEditor ? 2 : 1;
	while (drawcount-- > 0)
	{
		uint32_t vpos = SceneVertexPos;
		uint32_t ipos = SceneIndexPos;

		SceneVertex* vptr = Buffers->SceneVertices + vpos;
		uint32_t* iptr = Buffers->SceneIndexes + ipos;

		uint32_t istart = ipos;
		uint32_t icount = 0;

		for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
		{
			auto pts = Poly->Pts;
			uint32_t vcount = Poly->NumPts;
			if (vcount < 3) continue;

			for (uint32_t i = 0; i < vcount; i++)
			{
				FVector point = pts[i]->Point;
				FLOAT u = Facet.MapCoords.XAxis | point;
				FLOAT v = Facet.MapCoords.YAxis | point;

				vptr->Flags = flags;
				vptr->Position.x = point.X;
				vptr->Position.y = point.Y;
				vptr->Position.z = point.Z;
				vptr->TexCoord.s = (u - UPan) * UMult;
				vptr->TexCoord.t = (v - VPan) * VMult;
				vptr->TexCoord2.s = (u - LMUPan) * LMUMult;
				vptr->TexCoord2.t = (v - LMVPan) * LMVMult;
				vptr->TexCoord3.s = (u - MacroUPan) * MacroUMult;
				vptr->TexCoord3.t = (v - MacroVPan) * MacroVMult;
				vptr->TexCoord4.s = (u - DetailUPan) * DetailUMult;
				vptr->TexCoord4.t = (v - DetailVPan) * DetailVMult;
				vptr->Color = color;
				vptr->TextureBinds = ivec4{ 0, 0, 0, 0 };
				vptr++;
			}

			for (uint32_t i = vpos + 2; i < vpos + vcount; i++)
			{
				*(iptr++) = vpos;
				*(iptr++) = i - 1;
				*(iptr++) = i;
			}

			vpos += vcount;
			icount += (vcount - 2) * 3;
		}

		SceneVertexPos = vpos;
		SceneIndexPos = ipos + icount;

		if (drawcount != 0)
		{
			SetPipeline(RenderPasses->GetPipeline(PF_Highlighted));
			color = vec4(0.0f, 0.0f, 0.05f, 0.20f);
		}
	}

	Stats.ComplexSurfaces++;

	unguardSlow;
}

void UVulkanRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guardSlow(UVulkanRenderDevice::DrawGouraudPolygon);

	if (NumPts < 3) return; // This can apparently happen!!

	SetPipeline(RenderPasses->GetPipeline(PolyFlags));

	float UMult = GetUMult(Info);
	float VMult = GetVMult(Info);
	int flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;

	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && LightMode == 2) flags |= 32;

	if (PolyFlags & PF_Modulated)
	{
		SceneVertex* vertex = &Buffers->SceneVertices[SceneVertexPos];
		for (INT i = 0; i < NumPts; i++)
		{
			FTransTexture* P = Pts[i];
			vertex->Flags = flags;
			vertex->Position.x = P->Point.X;
			vertex->Position.y = P->Point.Y;
			vertex->Position.z = P->Point.Z;
			vertex->TexCoord.s = P->U * UMult;
			vertex->TexCoord.t = P->V * VMult;
			vertex->TexCoord2.s = P->Fog.X;
			vertex->TexCoord2.t = P->Fog.Y;
			vertex->TexCoord3.s = P->Fog.Z;
			vertex->TexCoord3.t = P->Fog.W;
			vertex->TexCoord4.s = 0.0f;
			vertex->TexCoord4.t = 0.0f;
			vertex->Color.r = 1.0f;
			vertex->Color.g = 1.0f;
			vertex->Color.b = 1.0f;
			vertex->Color.a = 1.0f;
			vertex->TextureBinds = ivec4{ 0, 0, 0, 0 };
			vertex++;
		}
	}
	else
	{
		SceneVertex* vertex = &Buffers->SceneVertices[SceneVertexPos];
		for (INT i = 0; i < NumPts; i++)
		{
			FTransTexture* P = Pts[i];
			vertex->Flags = flags;
			vertex->Position.x = P->Point.X;
			vertex->Position.y = P->Point.Y;
			vertex->Position.z = P->Point.Z;
			vertex->TexCoord.s = P->U * UMult;
			vertex->TexCoord.t = P->V * VMult;
			vertex->TexCoord2.s = P->Fog.X;
			vertex->TexCoord2.t = P->Fog.Y;
			vertex->TexCoord3.s = P->Fog.Z;
			vertex->TexCoord3.t = P->Fog.W;
			vertex->TexCoord4.s = 0.0f;
			vertex->TexCoord4.t = 0.0f;
			vertex->Color.r = P->Light.X;
			vertex->Color.g = P->Light.Y;
			vertex->Color.b = P->Light.Z;
			vertex->Color.a = 1.0f;
			vertex->TextureBinds = ivec4{ 0, 0, 0, 0 };
			vertex++;
		}
	}

	size_t vstart = SceneVertexPos;
	size_t vcount = NumPts;
	size_t istart = SceneIndexPos;
	size_t icount = (vcount - 2) * 3;

	uint32_t* iptr = Buffers->SceneIndexes + istart;
	for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
	{
		*(iptr++) = vstart;
		*(iptr++) = i - 1;
		*(iptr++) = i;
	}

	SceneVertexPos += vcount;
	SceneIndexPos += icount;

	Stats.GouraudPolygons++;

	unguardSlow;
}

void UVulkanRenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guardSlow(UVulkanRenderDevice::DrawTile);

	// stijn: fix for invisible actor icons in ortho viewports
	if (GIsEditor && Frame->Viewport->Actor && (Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
	{
		Z = 1.f;
	}

	if ((PolyFlags & (PF_Modulated)) == (PF_Modulated) && Info.Format == TEXF_P8)
		PolyFlags = PF_Modulated;

	//CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));

	SetPipeline(RenderPasses->GetPipeline(PolyFlags));

	float UMult = /*tex ? GetUMult(Info) :*/ 0.0f;
	float VMult = /*tex ? GetVMult(Info) :*/ 0.0f;

	SceneVertex* v = &Buffers->SceneVertices[SceneVertexPos];

	float r, g, b, a;
	if (PolyFlags & PF_Modulated)
	{
		r = 1.0f;
		g = 1.0f;
		b = 1.0f;
	}
	else
	{
		r = Color.X;
		g = Color.Y;
		b = Color.Z;
	}
	a = 1.0f;

	if (Textures->Scene->Multisample > 1)
	{
		XL = std::floor(X + XL + 0.5f);
		YL = std::floor(Y + YL + 0.5f);
		X = std::floor(X + 0.5f);
		Y = std::floor(Y + 0.5f);
		XL = XL - X;
		YL = YL - Y;
	}

	v[0] = { 0, vec3(RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y - Frame->FY2),      Z), vec2(U * UMult,        V * VMult),        vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), ivec4(1,1,1,1) };
	v[1] = { 0, vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y - Frame->FY2),      Z), vec2((U + UL) * UMult, V * VMult),        vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), ivec4(1,1,1,1) };
	v[2] = { 0, vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z), vec2((U + UL) * UMult, (V + VL) * VMult), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), ivec4(1,1,1,1) };
	v[3] = { 0, vec3(RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y + YL - Frame->FY2), Z), vec2(U * UMult,        (V + VL) * VMult), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), ivec4(1,1,1,1) };

	size_t vstart = SceneVertexPos;
	size_t vcount = 4;
	size_t istart = SceneIndexPos;
	size_t icount = (vcount - 2) * 3;

	uint32_t* iptr = Buffers->SceneIndexes + istart;
	for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
	{
		*(iptr++) = vstart;
		*(iptr++) = i - 1;
		*(iptr++) = i;
	}

	SceneVertexPos += vcount;
	SceneIndexPos += icount;

	Stats.Tiles++;

	unguardSlow;
}

vec4 UVulkanRenderDevice::ApplyInverseGamma(vec4 color)
{
	if (Viewport->IsOrtho())
		return color;
	float brightness = Clamp(Viewport->GetOuterUClient()->Brightness * 2.0, 0.05, 2.99);
	float gammaRed = Max(brightness + GammaOffset + GammaOffsetRed, 0.001f);
	float gammaGreen = Max(brightness + GammaOffset + GammaOffsetGreen, 0.001f);
	float gammaBlue = Max(brightness + GammaOffset + GammaOffsetBlue, 0.001f);
	return vec4(pow(color.r, gammaRed), pow(color.g, gammaGreen), pow(color.b, gammaBlue), color.a);
}

void UVulkanRenderDevice::Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UVulkanRenderDevice::Draw3DLine);

	P1 = P1.TransformPointBy(Frame->Coords);
	P2 = P2.TransformPointBy(Frame->Coords);
	if (Frame->Viewport->IsOrtho())
	{
		P1.X = (P1.X) / Frame->Zoom + Frame->FX2;
		P1.Y = (P1.Y) / Frame->Zoom + Frame->FY2;
		P1.Z = 1;
		P2.X = (P2.X) / Frame->Zoom + Frame->FX2;
		P2.Y = (P2.Y) / Frame->Zoom + Frame->FY2;
		P2.Z = 1;

		if (Abs(P2.X - P1.X) + Abs(P2.Y - P1.Y) >= 0.2)
		{
			Draw2DLine(Frame, Color, LineFlags, P1, P2);
		}
		else if (Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL)
		{
			Draw2DPoint(Frame, Color, LINE_None, P1.X - 1, P1.Y - 1, P1.X + 1, P1.Y + 1, P1.Z);
		}
	}
	else
	{
		SetPipeline(RenderPasses->GetLinePipeline(OccludeLines));

		//ivec4 textureBinds = SetDescriptorSet(PF_Highlighted, nullptr);

		SceneVertex* v = &Buffers->SceneVertices[SceneVertexPos];
		uint32_t* iptr = Buffers->SceneIndexes + SceneIndexPos;

		vec4 color = ApplyInverseGamma(vec4(Color.X, Color.Y, Color.Z, 1.0f));

		v[0] = { 0, vec3(P1.X, P1.Y, P1.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, ivec4{0,0,0,0} };
		v[1] = { 0, vec3(P2.X, P2.Y, P2.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, ivec4{0,0,0,0} };

		iptr[0] = SceneVertexPos;
		iptr[1] = SceneVertexPos + 1;

		SceneVertexPos += 2;
		SceneIndexPos += 2;
	}

	unguard;
}

void UVulkanRenderDevice::Draw2DClippedLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UVulkanRenderDevice::Draw2DClippedLine);
	URenderDevice::Draw2DClippedLine(Frame, Color, LineFlags, P1, P2);
	unguard;
}

void UVulkanRenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UVulkanRenderDevice::Draw2DLine);

	SetPipeline(RenderPasses->GetLinePipeline(OccludeLines));

	//ivec4 textureBinds = SetDescriptorSet(PF_Highlighted, nullptr);

	SceneVertex* v = &Buffers->SceneVertices[SceneVertexPos];
	uint32_t* iptr = Buffers->SceneIndexes + SceneIndexPos;

	vec4 color = ApplyInverseGamma(vec4(Color.X, Color.Y, Color.Z, 1.0f));

	v[0] = { 0, vec3(RFX2 * P1.Z * (P1.X - Frame->FX2), RFY2 * P1.Z * (P1.Y - Frame->FY2), P1.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, ivec4(1,1,1,1) };
	v[1] = { 0, vec3(RFX2 * P2.Z * (P2.X - Frame->FX2), RFY2 * P2.Z * (P2.Y - Frame->FY2), P2.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, ivec4(1,1,1,1) };

	iptr[0] = SceneVertexPos;
	iptr[1] = SceneVertexPos + 1;

	SceneVertexPos += 2;
	SceneIndexPos += 2;

	unguard;
}

void UVulkanRenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z)
{
	guard(UVulkanRenderDevice::Draw2DPoint);

	// Hack to fix UED selection problem with selection brush
	if (GIsEditor) Z = 1.0f;

	SetPipeline(RenderPasses->GetPointPipeline(OccludeLines));

	//ivec4 textureBinds = SetDescriptorSet(PF_Highlighted, nullptr);

	SceneVertex* v = &Buffers->SceneVertices[SceneVertexPos];

	vec4 color = ApplyInverseGamma(vec4(Color.X, Color.Y, Color.Z, 1.0f));

	v[0] = { 0, vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, ivec4(1,1,1,1) };
	v[1] = { 0, vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, ivec4(1,1,1,1) };
	v[2] = { 0, vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, ivec4(1,1,1,1) };
	v[3] = { 0, vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, ivec4(1,1,1,1) };

	size_t vstart = SceneVertexPos;
	size_t vcount = 4;
	size_t istart = SceneIndexPos;
	size_t icount = (vcount - 2) * 3;

	uint32_t* iptr = Buffers->SceneIndexes + istart;
	for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
	{
		*(iptr++) = vstart;
		*(iptr++) = i - 1;
		*(iptr++) = i;
	}

	SceneVertexPos += vcount;
	SceneIndexPos += icount;

	unguard;
}

void UVulkanRenderDevice::ClearZ(FSceneNode* Frame)
{
	guard(UVulkanRenderDevice::ClearZ);

	DrawBatch(Commands->GetDrawCommands());

	VkClearAttachment attachment = {};
	VkClearRect rect = {};
	attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	attachment.clearValue.depthStencil.depth = 1.0f;
	rect.layerCount = 1;
	rect.rect.extent.width = Textures->Scene->Width;
	rect.rect.extent.height = Textures->Scene->Height;
	Commands->GetDrawCommands()->clearAttachments(1, &attachment, 1, &rect);
	unguard;
}

void UVulkanRenderDevice::PushHit(const BYTE* Data, INT Count)
{
	guard(UVulkanRenderDevice::PushHit);
	debugf(L"PushHit: %d", Count);
	unguard;
}

void UVulkanRenderDevice::PopHit(INT Count, UBOOL bForce)
{
	guard(UVulkanRenderDevice::PopHit);
	debugf(L"PopHit: %d", Count);
	unguard;
}

void UVulkanRenderDevice::GetStats(TCHAR* Result)
{
	guard(UVulkanRenderDevice::GetStats);
	Result[0] = 0;
	unguard;
}

void UVulkanRenderDevice::ReadPixels(FColor* Pixels)
{
	guard(UVulkanRenderDevice::GetStats);

	// bruh

	unguard;
}

void UVulkanRenderDevice::EndFlash()
{
	guard(UVulkanRenderDevice::EndFlash);
	if (FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f))
	{
		vec4 color(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.0f, 1.0f));
		vec2 zero2(0.0f);
		ivec4 zero4(0);

		DrawBatch(Commands->GetDrawCommands());
		pushconstants.objectToProjection = mat4::identity();
		pushconstants.nearClip = vec4(0.0f, 0.0f, 0.0f, 1.0f);

		SetPipeline(RenderPasses->GetEndFlashPipeline());
		//SetDescriptorSet(DescriptorSets->GetTextureSet(0, nullptr));

		SceneVertex* v = &Buffers->SceneVertices[SceneVertexPos];

		v[0] = { 0, vec3(-1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };
		v[1] = { 0, vec3(1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };
		v[2] = { 0, vec3(1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };
		v[3] = { 0, vec3(-1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };

		size_t vstart = SceneVertexPos;
		size_t vcount = 4;
		size_t istart = SceneIndexPos;
		size_t icount = (vcount - 2) * 3;

		uint32_t* iptr = Buffers->SceneIndexes + istart;
		for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
		{
			*(iptr++) = vstart;
			*(iptr++) = i - 1;
			*(iptr++) = i;
		}

		SceneVertexPos += vcount;
		SceneIndexPos += icount;

		if (CurrentFrame)
			SetSceneNode(CurrentFrame);
	}
	unguard;
}

void UVulkanRenderDevice::SetSceneNode(FSceneNode* Frame)
{
	guardSlow(UVulkanRenderDevice::SetSceneNode);

	auto commands = Commands->GetDrawCommands();
	DrawBatch(commands);

	CurrentFrame = Frame;
	Aspect = Frame->FY / Frame->FX;
	RProjZ = (float)appTan(radians(Viewport->Actor->FovAngle) * 0.5);
	RFX2 = 2.0f * RProjZ / Frame->FX;
	RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;

	VkViewport viewportdesc = {};
	viewportdesc.x = Frame->XB;
	viewportdesc.y = Frame->YB;
	viewportdesc.width = Frame->X;
	viewportdesc.height = Frame->Y;
	viewportdesc.minDepth = 0.0f;
	viewportdesc.maxDepth = 1.0f;
	commands->setViewport(0, 1, &viewportdesc);

	pushconstants.objectToProjection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	pushconstants.nearClip = vec4(Frame->NearClip.X, Frame->NearClip.Y, Frame->NearClip.Z, Frame->NearClip.W);

	unguardSlow;
}

void UVulkanRenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags)
{
	guard(UVulkanRenderDevice::PrecacheTexture);
	//Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	unguard;
}

struct StagedTextureUpload {
	std::unique_ptr<VulkanBuffer> stagingBuffer;
	std::unique_ptr<VulkanImage> deviceImage;
	std::unique_ptr<VulkanImageView> imageView;
	int textureIndex;
	int usize, vsize;

	static StagedTextureUpload Create(VulkanDevice* device, UTexture* texture, int textureIndex) {
		// TODO: MipMaps. We may want to use native mip maps, but those
		//       are probably palletized and so we may get better results
		//       by generating them ourselves.

		if (texture->bParametric) {
			debugf(TEXT("Vulkan: StagedTextureUpload: Texture %s@%p is parametric"), texture->GetName(), texture);
		}

		if (texture->bRealtime) {
			debugf(TEXT("Vulkan: StagedTextureUpload: Texture %s@%p is realtime"), texture->GetName(), texture);
		}

		if (texture->Diffuse != 1) {
			debugf(TEXT("Vulkan: StagedTextureUpload: Texture %s@%p has unexpected diffuse %f"), texture->GetName(), texture, texture->Diffuse);
		}

		if (texture->Specular != 1) {
			debugf(TEXT("Vulkan: StagedTextureUpload: Texture %s@%p has unexpected specular %f"), texture->GetName(), texture, texture->Specular);
		}

		if (texture->Format != ETextureFormat::TEXF_P8) {
			debugf(TEXT("Vulkan: StagedTextureUpload: Texture %s@%p has unexpected format %d"), texture->GetName(), texture, texture->Format);
			throw std::runtime_error("Unsupported texture format");
		}

		auto masked = !!(texture->PolyFlags & PF_Masked);
		if (masked) {
			debugf(TEXT("Vulkan: StagedTextureUpload: Texture %s@%p is masked"), texture->GetName(), texture);
		}

		auto& mip = texture->Mips(0);
		mip.DataArray.Load();

		auto stagingBuffer = BufferBuilder()
			.Usage(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
			/*.MemoryType(
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)*/
			.Size(mip.USize * mip.VSize * 4)
			.Create(device);

		auto deviceImage = ImageBuilder()
			.Usage(
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
			.Size(mip.USize, mip.VSize)
			.Format(VK_FORMAT_R8G8B8A8_SRGB)
			.Create(device);

		// TODO: We should officially lock the texture here, but right now we probably
		//       don't care.
		auto stagingBufferData = static_cast<BYTE*>(stagingBuffer->Map(0, stagingBuffer->size));
		auto mipData = static_cast<BYTE*>(mip.DataArray.GetData());
		if (!mipData) {
			debugf(TEXT("Vulkan: StagedTextureUpload: Texture %s@%p has no data"), texture->GetName(), texture);
			throw std::runtime_error("Texture has no data");
		}
		bool hadTransparentPixels = false;
		for (int v = 0; v < mip.VSize; v++) {
			for (int u = 0; u < mip.USize; u++) {
				auto& palette = texture->Palette->Colors;
				auto color = palette(mipData[v * mip.USize + u]);
				*(stagingBufferData++) = color.R;
				*(stagingBufferData++) = color.G;
				*(stagingBufferData++) = color.B;
				if (masked)
					*(stagingBufferData++) = (color.R == 255 && color.B == 255) ? 0 : 255;
				else
					*(stagingBufferData++) = color.A;
				if (color.A != 255)
					hadTransparentPixels = true;
			}
		}
		stagingBuffer->Unmap();
		//mip.DataArray.Unload();

		if (hadTransparentPixels) {
			debugf(TEXT("Vulkan: StagedTextureUpload: Texture %s@%p has transparent pixels, flags: %x"), texture->GetName(), texture, texture->PolyFlags);
		}

		auto imageView = ImageViewBuilder()
			.Image(deviceImage.get(), VK_FORMAT_R8G8B8A8_SRGB)
			.Type(VK_IMAGE_VIEW_TYPE_2D)
			.Create(device);

		return {
			std::move(stagingBuffer),
			std::move(deviceImage),
			std::move(imageView),
			textureIndex,
			mip.USize,
			mip.VSize
		};
	}

	void transitionBeforeCopy(PipelineBarrier& barrier) {
		barrier.AddImage(
			deviceImage.get(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}

	void copy(VulkanCommandBuffer& commands) {
		VkBufferImageCopy copy{
			0,
			0,
			0,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
			{ 0, 0, 0 },
			{ usize, vsize, 1 }
		};
		commands.copyBufferToImage(stagingBuffer->buffer, deviceImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	}

	void transitionAfterCopy(PipelineBarrier& barrier) {
		barrier.AddImage(
			deviceImage.get(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);
	}
};

void collectTextures(std::set<UTexture*>& set, UTexture* base) {
	if (!base) return;
	if (set.find(base) != set.end()) return;
	set.insert(base);
	collectTextures(set, base->BumpMap);
	collectTextures(set, base->DetailTexture);
	collectTextures(set, base->MacroTexture);
	collectTextures(set, base->AnimCur);
	collectTextures(set, base->AnimNext);
}

static void collectActorsAndModelsAndMeshes(std::set<AActor*>& actors, std::set<UModel*>& models, std::set<UMesh*>& meshes, const ULevel* level) {
	for (int i = 0; i < level->Actors.Num(); i++) {
		auto actor = level->Actors(i);
		if (!actor) continue;
		if (actor->Brush) {
			actors.insert(actor);
			models.insert(actor->Brush);
		}
		if (actor->Mesh) {
			actors.insert(actor);
			meshes.insert(actor->Mesh);
		}
	}
}

static void collectTexturesFromModel(std::set<UTexture*>& set, const UModel* model) {
	for (int i = 0; i < model->Surfs.Num(); i++) {
		collectTextures(set, model->Surfs(i).Texture);
	}
}

static void collectTexturesFromMesh(std::set<UTexture*>& set, const UMesh* mesh) {
	for (int i = 0; i < mesh->Textures.Num(); i++) {
		collectTextures(set, mesh->Textures(i));
	}
}

struct ModelPusher {
	UTexture* defaultTexture;
	const std::map<UTexture*, StagedTextureUpload> & textureUploads;

	std::vector<Vertex> verts;
	std::vector<Wedge> wedges;
	std::vector<Surf> surfs;
	std::vector<UINT> surfIndices;
	std::vector<UINT> wedgeIndices;
	std::map<UModel*, ModelBase> modelBases;
	std::map<UMesh*, ModelBase> meshBases;

	ModelPusher(UTexture* defaultTexture, const std::map<UTexture*, StagedTextureUpload>& textureUploads)
		: defaultTexture(defaultTexture), textureUploads(textureUploads) {
	}

	void pushModel(UModel* model, ULevel* level) {
		const auto surfBase = surfs.size();
		const auto wedgeBase = wedges.size();
		const auto vertBase = verts.size();
		const auto wedgeIndexBase = wedgeIndices.size();

		FName lol{ L"un_CargoBoxMed", FNAME_Find };

		// push each point as a vert
		for (int i = 0; i < model->Points.Num(); i++) {
			auto point = model->Points(i);
			verts.push_back({
				point
			});
		}

		// push all the surfs
		for (int i = 0; i < model->Surfs.Num(); i++) {
			auto& surf = model->Surfs(i);
			auto texture = surf.Texture ? surf.Texture : defaultTexture;
			auto normal = model->Vectors(surf.vNormal);
			auto fcolor = surf.Texture ? surf.Texture->MipZero : FColor(255, 0, 255);
			auto foundTexture = textureUploads.find(texture);
			if (foundTexture == textureUploads.end())
			{
				debugf(TEXT("Vulkan: Texture %s@%p not found in texture uploads, we screwed up"), texture->GetFullName(), texture);
				throw std::runtime_error("Texture not found in texture uploads, we screwed up");
			}

			surfs.push_back(Surf{
				normal,
				foundTexture->second.textureIndex
			});
		}

		// now construct wedges from the vertices of each node
		// and build triangles on top of those
		for (int i = 0; i < model->Nodes.Num(); i++) {
			auto& node = model->Nodes(i);
			if (node.NumVertices < 3) continue;
			auto& surf = model->Surfs(node.iSurf);
			if (surf.PolyFlags & PF_Invisible) continue;
			//if (level->BrushTracker->SurfIsDynamic(node.iSurf)) continue;
			auto base = model->Points(surf.pBase);
			auto texture = surf.Texture ? surf.Texture : defaultTexture;
			if (texture->GetFName() == lol) {
				debugf(L"Found it");
			}
			auto texU = model->Vectors(surf.vTextureU) / texture->USize;
			auto texV = model->Vectors(surf.vTextureV) / texture->VSize;
			auto texBaseU = (base | texU) - surf.PanU / static_cast<float>(texture->USize);
			auto texBaseV = (base | texV) - surf.PanV / static_cast<float>(texture->VSize);
			auto nodeWedgeBase = wedges.size();

			// wedges
			for (int j = 0; j < node.NumVertices; j++) {
				auto point = model->Points(model->Verts(node.iVertPool + j).pVertex);

				wedges.push_back(Wedge {
					(point | texU) - texBaseU,
					(point | texV) - texBaseV,
					vertBase + model->Verts(node.iVertPool + j).pVertex,
				});
			}

			// push the triangle indices
			for (int j = 2; j < node.NumVertices; j++) {
				surfIndices.push_back(surfBase + node.iSurf);
				wedgeIndices.push_back(nodeWedgeBase + 0);
				wedgeIndices.push_back(nodeWedgeBase + j - 1);
				wedgeIndices.push_back(nodeWedgeBase + j);
			}
		}

		auto numSurfs = surfs.size() - surfBase;
		auto numWedges = wedges.size() - wedgeBase;
		auto numVerts = verts.size() - vertBase;
		auto numWedgeIndices = wedgeIndices.size() - wedgeIndexBase;
		modelBases[model] = { wedgeIndexBase, numWedgeIndices };
		debugf(L"Vulkan: %s@%p: Pushed %d surfs, %d wedges, %d verts and %d wedge indices, model starts at %d", model->GetFullName(), model, numSurfs, numWedges, numVerts, numWedgeIndices, wedgeIndexBase);
	}

	void pushMesh(UMesh* mesh) {
		const auto surfBase = surfs.size();
		const auto wedgeBase = wedges.size();
		const auto vertBase = verts.size();
		const auto wedgeIndexBase = wedgeIndices.size();
		FName dxchars(L"DeusExCharacters", FNAME_Find);
		bool isDxChar = mesh->GetOuter() && mesh->GetOuter()->GetFName() == dxchars;
		auto fullName = mesh->GetFullName();
		if (isDxChar) {
			debugf(L"Vulkan: %s@%p: Is a dx char", fullName, mesh);
		}
		
		FCoords xform(FVector(0, 0, 0));
		//xform *= lodMesh->RotOrigin;
		xform *= FScale(mesh->Scale, 0.f, SHEER_None);
		xform /= mesh->Origin;
		xform *= mesh->RotOrigin;

		if (mesh->IsA(ULodMesh::StaticClass())) {
			ULodMesh* lodMesh = static_cast<ULodMesh*>(mesh);
			debugf(L"Vulkan: %s@%p: FrameVerts=%d, AnimFrames=%d, ModelVerts=%d, SpecialVerts=%d, OldFrameVerts=%d, Verts=%d, Tris=%d, AnimSeqs=%d, Connects=%d, BoundingBoxes=%d, BoundingSpheres=%d, Vertlinks=%d, Textures=%d, TextureLOD=%d, CollapsePointThus=%d, FaceLevel=%d, Faces=%d, CollapseWedgeThus=%d, Wedges=%d, Materials=%d, SpecialFaces=%d, RemapAnimVerts=%d",
				lodMesh->GetFullName(), lodMesh, lodMesh->FrameVerts, lodMesh->AnimFrames, lodMesh->ModelVerts, lodMesh->SpecialVerts, lodMesh->OldFrameVerts, lodMesh->Verts.Num(), lodMesh->Tris.Num(), lodMesh->AnimSeqs.Num(),
				lodMesh->Connects.Num(), lodMesh->BoundingBoxes.Num(), lodMesh->BoundingSpheres.Num(), lodMesh->VertLinks.Num(), lodMesh->Textures.Num(), lodMesh->TextureLOD.Num(), lodMesh->CollapsePointThus.Num(), lodMesh->FaceLevel.Num(),
				lodMesh->Faces.Num(), lodMesh->CollapseWedgeThus.Num(), lodMesh->Wedges.Num(), lodMesh->Materials.Num(), lodMesh->SpecialFaces.Num(), lodMesh->RemapAnimVerts.Num());

			// push all the non-special verts as verts
			// wedges now index into this
			for (int i = lodMesh->SpecialVerts; i < lodMesh->Verts.Num(); i++) {
				auto point = lodMesh->Verts(i).Vector();
				verts.push_back({
					point.TransformPointBy(xform)
				});
			}

			// push all the wedges as wedge
			for (int i = 0; i < lodMesh->Wedges.Num(); i++) {
				auto wedge = lodMesh->Wedges(i);
				wedges.push_back({
					wedge.TexUV.U / 255.0f, // hopium // seems to work, can we confirm that?
					wedge.TexUV.V / 255.0f,
					vertBase + wedge.iVertex,
				});
			}

			// push all the faces as surfs & triangles
			for (int i = 0; i < lodMesh->Faces.Num(); i++) {
				//if (isDxChar && i > 10) {
				//	debugf(L"wtf");
				//	break;
				//}
				auto& face = lodMesh->Faces(i);
				auto material = lodMesh->Materials(face.MaterialIndex);
				//if (material.PolyFlags != 0)
				//	debugf(L"Vulkan: LOD mesh %s@%p face %d with texture %s@%p has flags %d", lodMesh->GetFullName(), lodMesh, i, texture->GetFullName(), texture, material.PolyFlags);
				auto surfIdx = surfs.size();
				surfs.push_back({
					{ 1, 0, 0 },
					//(static_cast<UINT>(fcolor.R) << 16) | (fcolor.G << 8) | fcolor.B,
					resolveTextureIndexForMesh(lodMesh, material.TextureIndex)
				});

				// push each face as a triangle
				surfIndices.push_back(surfIdx);
				for (int j = 0; j < 3; j++) {
					wedgeIndices.push_back(wedgeBase + face.iWedge[j]);
				}
			}
		}
		else {
			debugf(L"Vulkan: %s@%p: FrameVerts=%d, AnimFrames=%d, Verts=%d, Tris=%d, AnimSeqs=%d, Connects=%d, BoundingBoxes=%d, BoundingSpheres=%d, Vertlinks=%d, Textures=%d, TextureLOD=%d",
				mesh->GetFullName(), mesh, mesh->FrameVerts, mesh->AnimFrames, mesh->Verts.Num(), mesh->Tris.Num(), mesh->AnimSeqs.Num(), mesh->Connects.Num(), mesh->BoundingBoxes.Num(), mesh->BoundingSpheres.Num(), mesh->VertLinks.Num(), mesh->Textures.Num(), mesh->TextureLOD.Num());

			// push all the verts
			for (int i = 0; i < mesh->Verts.Num(); i++) {
				auto point = mesh->Verts(i).Vector();
				verts.push_back({
					point.TransformPointBy(xform)
				});
			}

			// for each tri, push three wedges, a surf and appropriate indices
			for (int i = 0; i < mesh->Tris.Num(); i++) {
				auto& tri = mesh->Tris(i);

				surfIndices.push_back(surfs.size());
				surfs.push_back({
					{ 1, 0, 0 },
					resolveTextureIndexForMesh(mesh, tri.TextureIndex)
				});

				for (int j = 0; j < 3; j++) {
					wedgeIndices.push_back(wedges.size());
					wedges.push_back({
						tri.Tex[j].U / 255.0f,
						tri.Tex[j].V / 255.0f,
						vertBase + tri.iVertex[j]
					});
				}
			}
		}

		auto numSurfs = surfs.size() - surfBase;
		auto numWedges = wedges.size() - wedgeBase;
		auto numVerts = verts.size() - vertBase;
		auto numWedgeIndices = wedgeIndices.size() - wedgeIndexBase;
		meshBases[mesh] = { wedgeIndexBase, numWedgeIndices };
		debugf(L"Vulkan: %s@%p: Pushed %d surfs, %d wedges, %d verts and %d wedge indices, model starts at %d", mesh->GetFullName(), mesh, numSurfs, numWedges, numVerts, numWedgeIndices, wedgeIndexBase);
	}
private:
	int resolveTextureIndexForMesh(UMesh* mesh, int textureIndex) {
		if (textureIndex < 0) {
			debugf(L"Vulkan: %s@%p: Negative texture index %d", mesh->GetFullName(), mesh, textureIndex);
			throw std::runtime_error("Negative texture index");
		}
		if (textureIndex >= 8) {
			debugf(L"Vulkan: %s@%p: Texture index %d out of bounds, we only support up to eight textures per mesh", mesh->GetFullName(), mesh, textureIndex);
			throw std::runtime_error("Texture index out of bounds");
		}
		if (textureIndex >= mesh->Textures.Num()) {
			debugf(L"Vulkan: %s@%p: Texture index %d out of bounds, we only have %d textures", mesh->GetFullName(), mesh, textureIndex, mesh->Textures.Num());
			throw std::runtime_error("Texture index out of bounds");
		}
		// negative texture indices are remapped in the vertex shader
		// based on the actor's textures
		return -textureIndex - 1;
	}
};

class FakeMovingBrushTracker : public FMovingBrushTrackerBase {
public:
	// Public operations:
	void Update(AActor* Actor);
	void Flush(AActor* Actor);
	UBOOL SurfIsDynamic(INT iSurf);
	void CountBytes(FArchive& Ar);
};

static int getVertexOffsetFromActor(UMesh* mesh, AActor* actor) {
	if (actor->Owner && actor->bAnimByOwner) {
		actor = actor->Owner;
	}

	auto animSeq = mesh->GetAnimSeq(actor->AnimSequence);
	if (!animSeq) return 0;

	auto frameFloatIdx = actor->AnimFrame * animSeq->NumFrames;
	if (frameFloatIdx < 0) frameFloatIdx = 0;
	auto frameIdx = static_cast<int>(std::round(frameFloatIdx));
	frameIdx = frameIdx % animSeq->NumFrames;
	return (animSeq->StartFrame + frameIdx) * mesh->FrameVerts;
}

void UVulkanRenderDevice::DrawWorld(FSceneNode* scene)
{
	guard(UVulkanRenderDevice::DrawWorld);
	bool firstTime = false;
	if (lastScene && lastScene->level != scene->Level)
	{
		debugf(TEXT("Vulkan: Scene changed, resetting"));
		vkDeviceWaitIdle(Device->device);
		lastScene.reset();
		firstTime = true;
	}

	if (!lastScene) try {
		debugf(TEXT("Vulkan: Scene changed, gonna upload data to GPU"));
		auto level = scene->Level;
		//auto model = level->Model;
		debugf(L"Vulkan: Scene %p, Level %s@%p, Model %s@%p", scene, scene->Level->GetFullName(), scene->Level, level->Model->GetFullName(), level->Model);

		// Clean up moving brushes.
		// Actors with a brush-based model are actually inserted into
		// the level's BSP tree. That's annoying, as we want to handle
	    // the level geometry as being entirely static and just move
		// the objects within it.
		// To achieve that goal, we must not insert geometry from these
		// brush-based actors into our level model. We could do that by
		// filtering the surfs out. Or, we could go the nuclear route
		// and just remove all moving brushes from the level and then
		// install a fake moving brush tracker that does nothing.
		// TODO: Does this break anything? Ćollisions, perhaps? We'll see.
		for (int i = 0; i < level->Actors.Num(); i++) {
			auto actor = level->Actors(i);
			if (!actor) continue;
			if (actor && actor->IsMovingBrush()) {
				level->BrushTracker->Flush(actor);
			}
		}
		delete level->BrushTracker;
		level->BrushTracker = new FakeMovingBrushTracker();

		auto defaultTexture = scene->Viewport->Actor->Level->DefaultTexture;
		if (!defaultTexture) {
			debugf(TEXT("Vulkan: No default texture found"));
			throw std::runtime_error("No default texture found");
		}

		// collect all actors, models & meshes
		std::set<AActor*> actors;
		/*std::set<UModel*> models;
		std::set<UMesh*> meshes;
		models.insert(level->Model);
		collectActorsAndModelsAndMeshes(actors, models, meshes, level);*/

		std::set<UMesh*> meshes;
		for (TObjectIterator<UMesh> meshIt; meshIt; ++meshIt) {
			meshes.insert(*meshIt);
		}

		std::set<UModel*> models;
		//models.insert(level->Model);
		for (TObjectIterator<UModel> modelIt; modelIt; ++modelIt) {
			models.insert(*modelIt);
		}

		std::set<UTexture*> textures;
		for (TObjectIterator<UTexture> textureIt; textureIt; ++textureIt) {
			textures.insert(*textureIt);
		}

		debugf(L"Object iterators found %d meshes, %d models and %d textures", meshes.size(), models.size(), textures.size());


		auto numTexturesBeforeCollection = textures.size();
		// collect all textures from all models
		collectTextures(textures, defaultTexture);
		for (auto model : models) {
			collectTexturesFromModel(textures, model);
		}

		// collect all textures from all meshes
		for (auto mesh : meshes) {
			collectTexturesFromMesh(textures, mesh);
		}

		debugf(L"Texture collection found extra %d textures", textures.size() - numTexturesBeforeCollection);

		// prepare texture uploads
		std::map<UTexture*, StagedTextureUpload> textureUploads;
		for (auto texture : textures)
		{
			debugf(TEXT("Vulkan: Preparing texture %s@%p for upload"), texture->GetFullName(), texture);
			textureUploads[texture] = StagedTextureUpload::Create(Device.get(), texture, textureUploads.size());;
		}

		ModelPusher modelPusher(defaultTexture, textureUploads);

		// count all surfs & verts
		for (auto model : models) {
			modelPusher.pushModel(model, level);
		}
		for (auto mesh : meshes) {
			// gotta load the mesh data
			mesh->Tris.Load();
			mesh->Verts.Load();
			modelPusher.pushMesh(mesh);
		}

		if (modelPusher.wedgeIndices.size() != modelPusher.surfIndices.size() * 3) {
			debugf(L"Vulkan: We screwed up, we expected to have 3 wedge indices per surf index, but got %d vert indices and %d surf indices", modelPusher.wedgeIndices.size(), modelPusher.surfIndices.size());
			throw std::runtime_error("We screwed up, we expected to have 3 vert indices per surf index");
		}

		debugf(L"Vulkan: Done pushing local buffers, got %d surfs, %d wedges, %d verts, %d surf indices and %d wedge indices",
			modelPusher.surfs.size(), modelPusher.wedges.size(), modelPusher.verts.size(), modelPusher.surfIndices.size(), modelPusher.wedgeIndices.size());

		// check if we got the indices right
		for (auto wedge : modelPusher.wedges) {
			if (wedge.vertIndex > modelPusher.verts.size()) {
				debugf(L"Vulkan: We screwed up, we have %d verts, but got a wedge with vert index %d", modelPusher.verts.size(), wedge.vertIndex);
				throw std::runtime_error("We screwed up vert indices in wedges");
			}
		}

		for (auto surfIndex : modelPusher.surfIndices) {
			if (surfIndex >= modelPusher.surfs.size()) {
				debugf(L"Vulkan: We screwed up, we have %d surfs, but got a surf index %d", modelPusher.surfs.size(), surfIndex);
				throw std::runtime_error("We screwed up surf indices");
			}
		}

		for (auto wedgeIndex : modelPusher.wedgeIndices) {
			if (wedgeIndex >= modelPusher.wedges.size()) {
				debugf(L"Vulkan: We screwed up, we have %d wedges, but got a wedge index %d", modelPusher.wedges.size(), wedgeIndex);
				throw std::runtime_error("We screwed up wedge indices");
			}
		}

		// create & fill staging buffers
		auto surfUpload = StagedUpload<Surf>::create(Device.get(), modelPusher.surfs.size(), "SurfBuffer");
		surfUpload.fillFrom(std::move(modelPusher.surfs));
		auto wedgeUpload = StagedUpload<Wedge>::create(Device.get(), modelPusher.wedges.size(), "WedgeBuffer");
		wedgeUpload.fillFrom(std::move(modelPusher.wedges));
		auto vertUpload = StagedUpload<Vertex>::create(Device.get(), modelPusher.verts.size(), "VertexBuffer");
		vertUpload.fillFrom(std::move(modelPusher.verts));
		auto surfIdxUpload = StagedUpload<UINT>::create(Device.get(), modelPusher.surfIndices.size(), "SurfIndexBuffer");
		surfIdxUpload.fillFrom(std::move(modelPusher.surfIndices));
		auto wedgeIdxUpload = StagedUpload<UINT>::create(Device.get(), modelPusher.wedgeIndices.size(), "WedgeIndexBuffer");
		wedgeIdxUpload.fillFrom(std::move(modelPusher.wedgeIndices));


		debugf(L"Vulkan: Finished filling surf, wedge, vert, surf index and wedge index buffers");


		// upload all the data
		debugf(TEXT("Vulkan: Building command buffers"));
		auto uploadCommands = Commands->CreateCommandBuffer();
		uploadCommands->begin();
		{
			// barrier before texture copy
			auto barrier = PipelineBarrier();
			for (auto& [texture, upload] : textureUploads)
				upload.transitionBeforeCopy(barrier);
			barrier.Execute(
				uploadCommands.get(),
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT
			);
		}
		for (auto& [texture, upload] : textureUploads)
		{
			upload.copy(*uploadCommands);
		}
		{
			// barrier after texture copy
			auto barrier = PipelineBarrier();
			for (auto& [texture, upload] : textureUploads)
				upload.transitionAfterCopy(barrier);
			barrier.Execute(
				uploadCommands.get(),
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			);
		}

		surfUpload.copy(*uploadCommands);
		wedgeUpload.copy(*uploadCommands);
		vertUpload.copy(*uploadCommands);
		surfIdxUpload.copy(*uploadCommands);
		wedgeIdxUpload.copy(*uploadCommands);
		uploadCommands->end();

		VulkanFence fence(Device.get());

		debugf(TEXT("Vulkan: Submitting upload commands"));
		QueueSubmit()
			.AddCommandBuffer(uploadCommands.get())
			.Execute(Device.get(), Device->GraphicsQueue, &fence);

		debugf(TEXT("Vulkan: Waiting for upload to finish"));
		// TODO: Waiting for the upload is a bit sketchy, but it happens only
		//       when the level changes, so we're probably fine.
		vkWaitForFences(Device->device, 1, &fence.fence, VK_TRUE, UINT64_MAX);
		debugf(TEXT("Vulkan: Upload finished"));

		auto uploadedTextures = std::map<UTexture*, UploadedTexture>();
		std::vector<VulkanImageView*> allTextureViews;
		for (auto& [texture, upload] : textureUploads)
		{
			allTextureViews.push_back(upload.imageView.get());
			uploadedTextures[texture] = {
				std::move(upload.deviceImage),
				std::move(upload.imageView),
				upload.textureIndex
			};
		}

		auto maxNumObjects = level->Actors.Num() * 4;
		lastScene = LastScene{
			scene->Level,
			std::move(surfUpload.deviceBuffer),
			std::move(wedgeUpload.deviceBuffer),
			std::move(vertUpload.deviceBuffer),
			std::move(surfIdxUpload.deviceBuffer),
			std::move(wedgeIdxUpload.deviceBuffer),
			std::move(modelPusher.modelBases),
			std::move(modelPusher.meshBases),
			std::move(uploadedTextures),
			maxNumObjects,
			{
				{
					StagedUpload<Object>::create(Device.get(), maxNumObjects, "OddObjectBuffer", VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
					Commands->CreateCommandBuffer()
				},
				{
					StagedUpload<Object>::create(Device.get(), maxNumObjects, "EvenObjectBuffer", VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
					Commands->CreateCommandBuffer()
				}
			},
			false
		};

		WriteDescriptors writeDescriptors;
		for (int i = 0; i < 2; i++) {
			auto& perFrame = lastScene->perFrame[i];
			auto descriptorSet = DescriptorSets->GetNewSet(!!i);
			writeDescriptors
				.AddBuffer(descriptorSet, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lastScene->surfBuffer.get())
				.AddBuffer(descriptorSet, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lastScene->wedgeBuffer.get())
				.AddBuffer(descriptorSet, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lastScene->vertBuffer.get())
				.AddBuffer(descriptorSet, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, perFrame.objectUpload.deviceBuffer.get())
				.AddBuffer(descriptorSet, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lastScene->surfIdxBuffer.get())
				.AddBuffer(descriptorSet, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, lastScene->wedgeIdxBuffer.get())
				.AddSampler(descriptorSet, 6, Samplers->Samplers[0].get())
				.AddImageArray(descriptorSet, 7, allTextureViews, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			perFrame.objectUploadCommands->begin(0);
			perFrame.objectUpload.copy(*perFrame.objectUploadCommands);
			PipelineBarrier()
				.AddBuffer(perFrame.objectUpload.deviceBuffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT )
				.Execute(perFrame.objectUploadCommands.get(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
			perFrame.objectUploadCommands->end();
		}

		writeDescriptors.Execute(Device.get());
	}
	catch (std::exception& e) {
		debugf(TEXT("Vulkan: Failed to upload scene data because: %S"), e.what());
		throw;
	}
	catch (...) {
		std::exception_ptr e = std::current_exception();

		debugf(TEXT("Vulkan: Failed to upload scene data"));
		throw;
	}

	auto oddEven = lastScene->oddEven;
	auto defaultTextureIndex = lastScene->textureBuffers.at(scene->Viewport->Actor->Level->DefaultTexture).index;
	auto& perFrame = lastScene->perFrame[oddEven];
	UINT actorIdx = 0;
	{
		auto objectBuffer = perFrame.objectUpload.map();
		auto levelModelBase = lastScene->modelBases.at(lastScene->level->Model);
		objectBuffer[actorIdx++] = {
			mat4::identity(),
			{}, // level has no texture remapping
			0, // level draws with no vertex offset
			{}, // pad
			VkDrawIndirectCommand{
				levelModelBase.wedgeIndexCount,
				1,
				levelModelBase.wedgeIndexBase,
				0
			}
		};

		auto& actors = scene->Level->Actors;
		if (actors.Num() > lastScene->maxNumObjects) {
			debugf(TEXT("Vulkan: Too many actors in scene, expected %d, got %d"), lastScene->maxNumObjects, actors.Num());
			throw std::runtime_error("Too many actors in scene");
		}
		FName dxchars(L"DeusExCharacters", FNAME_Find);
		// excluded actor (i.e. typically the player)
		auto excludedActor = (Viewport->Actor->bBehindView || scene->Parent != nullptr) ? nullptr :
			Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
		// the playerActor, used to identify the player's weapon
		auto playerActor = scene->Viewport->Actor->ViewTarget ? Cast<APawn>(scene->Viewport->Actor->ViewTarget)
			: scene->Viewport->Actor->bBehindView ? nullptr
			: scene->Viewport->Actor;
		for (int i = 0; i < actors.Num(); i++) {
			auto actor = actors(i);
			if (!actor) continue;
			if (playerActor && playerActor->Weapon == actor) {
				// TODO: This is definitely wrong, but right now it shall serve as a good enough approximation.
				// We should figure out how the game actually does this. AActor's RenderOverlays, perhaps?
				auto playerActorRot = playerActor->GetViewRotation();
				actor->Rotation = playerActorRot;
			}
			else if (actor->bHidden) continue;
			if (actor == excludedActor) continue;
			auto modelBase = lastScene->modelBaseForActor(actor);
			if (!modelBase) continue;
			auto prePivot = mat4::translate(-actor->PrePivot.X, -actor->PrePivot.Y, -actor->PrePivot.Z);
			auto translation = mat4::translate(actor->Location.X, actor->Location.Y, actor->Location.Z);
			auto rotation = mat4::rotate(2 * PI * actor->Rotation.Yaw / 65536., 0, 0, 1)
				* mat4::rotate(2 * PI * actor->Rotation.Pitch / 65536., 1, 0, 0)
				* mat4::rotate(2 * PI * actor->Rotation.Roll / 65536., 0, 1, 0);
			Object object{
				translation * rotation * prePivot,
				{},
				0,
				{}, // pad
				VkDrawIndirectCommand{
						modelBase->wedgeIndexCount,
						1,
						modelBase->wedgeIndexBase,
						actorIdx
					},
			};
			if (actor->Mesh) {
				auto mesh = actor->Mesh;

				for (int i = 0; i < mesh->Textures.Num(); i++) {
					auto texture = mesh->GetTexture(i, actor);
					if (texture) {
						auto foundTexture = lastScene->textureBuffers.find(texture);
						if (foundTexture == lastScene->textureBuffers.end()) {
							debugf(L"Vulkan: Texture %s@%p (index %d) for %s@%p not found", texture->GetFullName(), texture, i, actor->GetFullName(), actor);
							throw std::runtime_error("Texture not found");
						}
						if (firstTime)
							debugf(L"Vulkan: %s@%p: Has texture %s@%p, index %d, mapped to %d", actor->GetFullName(), actor, texture->GetFullName(), texture, i, foundTexture->second.index);
						object.textures[i] = foundTexture->second.index;
					}
					else {
						object.textures[i] = defaultTextureIndex;
					}
				}

				object.vertexOffset = getVertexOffsetFromActor(mesh, actor);
			}
			
			objectBuffer[actorIdx++] = object;
		}
	}
	perFrame.objectUpload.unmap();
	QueueSubmit()
		.AddCommandBuffer(perFrame.objectUploadCommands.get())
		.Execute(Device.get(), Device->GraphicsQueue, nullptr);

	auto coords = scene->Coords;
	auto subtractOriginMatrix = mat4{
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		-coords.Origin.X, -coords.Origin.Y, -coords.Origin.Z, 1
	};
	auto axisMatrix = mat4{
		coords.XAxis.X, coords.YAxis.X, coords.ZAxis.X, 0,
		coords.XAxis.Y, coords.YAxis.Y, coords.ZAxis.Y, 0,
		coords.XAxis.Z, coords.YAxis.Z, coords.ZAxis.Z, 0,
		0, 0, 0, 1
	};

	auto push = NewScenePushConstants{
		pushconstants.objectToProjection * axisMatrix * subtractOriginMatrix,
	};
	auto layout = RenderPasses->Scene.NewPipelineLayout.get();
	auto cmdBuf = Commands->GetDrawCommands();
	cmdBuf->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->Scene.NewPipeline.get());
	cmdBuf->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, DescriptorSets->GetNewSet(oddEven));
	cmdBuf->pushConstants(layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(NewScenePushConstants), &push);
	cmdBuf->drawIndirect(
		perFrame.objectUpload.deviceBuffer->buffer,
		offsetof(Object, command),
		actorIdx,
		sizeof(Object)
	);

	lastScene->oddEven = !lastScene->oddEven;
	unguard;
}

std::optional<ModelBase> UVulkanRenderDevice::LastScene::modelBaseForActor(const AActor* actor) {
	if (actor->Brush) {
		auto found = modelBases.find(actor->Brush);
		if (found != modelBases.end()) {
			return found->second;
		}
		if (missingModels.find(actor->Brush) == missingModels.end()) {
			missingModels.insert(actor->Brush);
			debugf(L"Vulkan: Model %s@%p for %s@%p not found", actor->Brush->GetFullName(), actor->Brush, actor->GetFullName(), actor);
		}
	}
	else if (actor->Mesh) {
		auto found = meshBases.find(actor->Mesh);
		if (found != meshBases.end()) {
			return found->second;
		}
		if (missingMeshes.find(actor->Mesh) == missingMeshes.end()) {
			missingMeshes.insert(actor->Mesh);
			debugf(L"Vulkan: Mesh %s@%p for %s@%p not found", actor->Mesh->GetFullName(), actor->Mesh, actor->GetFullName(), actor);
		}
	}
	return std::nullopt;
}

void FakeMovingBrushTracker::Update(AActor* Actor) {
	guard(FakeMovingBrushTracker::Update);
	debugf(TEXT("Vulkan: FakeMovingBrushTracker::Update %s@%p"), Actor->GetFullName(), Actor);
	unguard;
}

void FakeMovingBrushTracker::Flush(AActor* Actor) {
	guard(FakeMovingBrushTracker::Flush);
	debugf(TEXT("Vulkan: FakeMovingBrushTracker::Flush %s@%p"), Actor->GetFullName(), Actor);
	unguard;
}

UBOOL FakeMovingBrushTracker::SurfIsDynamic(INT iSurf) {
	guard(FakeMovingBrushTracker::SurfIsDynamic);
	debugf(TEXT("Vulkan: FakeMovingBrushTracker::SurfIsDynamic %d"), iSurf);
	return false;
	unguard;
}

void FakeMovingBrushTracker::CountBytes(FArchive& Ar) {
	guard(FakeMovingBrushTracker::CountBytes);
	debugf(TEXT("Vulkan: FakeMovingBrushTracker::CountBytes"));
	unguard;
}
