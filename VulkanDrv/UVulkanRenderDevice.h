#pragma once

#include "Precomp.h"
#include <optional>
#include "CommandBufferManager.h"
#include "BufferManager.h"
#include "DescriptorSetManager.h"
#include "FramebufferManager.h"
#include "RenderPassManager.h"
#include "SamplerManager.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "UploadManager.h"
#include "vec.h"
#include "mat.h"
#include "types.h"

class CachedTexture;

struct ModelBase {
	UINT wedgeIndexBase;
	UINT wedgeIndexCount;
};

template<typename T>
struct StagedUpload
{
	std::unique_ptr<VulkanBuffer> staging_buffer;
	std::unique_ptr<VulkanBuffer> device_buffer;

	static StagedUpload<T> create(VulkanDevice* device, size_t count, const char* debugName, VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
		auto buffer_size = count * sizeof(T);
		auto staging_buffer = BufferBuilder()
			.Usage(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
			/*.MemoryType(
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)*/
			.Size(buffer_size)
			.MinAlignment(16)
			.DebugName("StagingBuffer")
			.Create(device);

		auto device_buffer = BufferBuilder()
			.Usage(
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | usageFlags,
				VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE)
			.Size(buffer_size)
			.MinAlignment(16)
			.DebugName(debugName)
			.Create(device);

		return { std::move(staging_buffer), std::move(device_buffer) };
	}

	static StagedUpload<T> create(VulkanDevice* device, const std::vector<T>& data, const char* debugName, VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
		auto upload = create(device, data.size(), debugName, usageFlags);
		upload.fill_from(data);
		return upload;
	}

	T* map() {
		return static_cast<T*>(staging_buffer->Map(0, staging_buffer->size));
	}

	void unmap() {
		staging_buffer->Unmap();
	}

	void fill_from(const std::vector<T>& data) {
		auto buffer = map();
		memcpy(buffer, data.data(), data.size() * sizeof(T));
		unmap();
	}

	void copy(VulkanCommandBuffer& commands) {
		commands.copyBuffer(staging_buffer.get(), device_buffer.get());
	}
};

struct UploadedTexture
{
	std::unique_ptr<VulkanImage> image;
	std::unique_ptr<VulkanImageView> view;
	int index;
};

#if defined(OLDUNREAL469SDK)
class UVulkanRenderDevice : public URenderDeviceOldUnreal469
{
public:
	DECLARE_CLASS(UVulkanRenderDevice, URenderDeviceOldUnreal469, CLASS_Config, VulkanDrv)
#else
class UVulkanRenderDevice : public URenderDevice
{
public:
	DECLARE_CLASS(UVulkanRenderDevice, URenderDevice, CLASS_Config)
#endif

	UVulkanRenderDevice();
	void StaticConstructor();

	UBOOL Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) override;
	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) override;
	void Exit() override;
#if defined(UNREALGOLD)
	void Flush() override;
#else
	void Flush(UBOOL AllowPrecache) override;
#endif
	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);
	void Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize) override;
	void Unlock(UBOOL Blit) override;
	void DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet) override;
	void DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span) override;
	void DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags) override;
	void Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector OrigP, FVector OrigQ);
	void Draw2DClippedLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) override;
	void Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z) override;
	void ClearZ(FSceneNode* Frame) override;
	void PushHit(const BYTE* Data, INT Count) override;
	void PopHit(INT Count, UBOOL bForce) override;
	void GetStats(TCHAR* Result) override;
	void ReadPixels(FColor* Pixels) override;
	void EndFlash() override;
	void SetSceneNode(FSceneNode* Frame) override;
	void PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags) override;
	void DrawStats(FSceneNode* Frame) override;

	// used directly by UVkRender
	void DrawWorld(FSceneNode* frame);

	int InterfacePadding[64]; // For allowing URenderDeviceOldUnreal469 interface to add things

	HWND WindowHandle = 0;
	std::shared_ptr<VulkanDevice> Device;

	std::unique_ptr<CommandBufferManager> Commands;

	std::unique_ptr<SamplerManager> Samplers;
	std::unique_ptr<TextureManager> Textures;
	std::unique_ptr<BufferManager> Buffers;
	std::unique_ptr<ShaderManager> Shaders;
	std::unique_ptr<UploadManager> Uploads;

	std::unique_ptr<DescriptorSetManager> DescriptorSets;
	std::unique_ptr<RenderPassManager> RenderPasses;
	std::unique_ptr<FramebufferManager> Framebuffers;

	// Configuration.
	BITFIELD UseVSync;
	FLOAT GammaOffset;
	FLOAT GammaOffsetRed;
	FLOAT GammaOffsetGreen;
	FLOAT GammaOffsetBlue;
	BYTE LinearBrightness;
	BYTE Contrast;
	BYTE Saturation;
	INT GrayFormula;
	BITFIELD Hdr;
	BITFIELD OccludeLines;
	BITFIELD Bloom;
	BYTE BloomAmount;
	FLOAT LODBias;
	BYTE AntialiasMode;
	BYTE GammaMode;
	BYTE LightMode;

	INT VkDeviceIndex;
	BITFIELD VkDebug;
	BITFIELD VkExclusiveFullscreen;

	struct
	{
		int ComplexSurfaces = 0;
		int GouraudPolygons = 0;
		int Tiles = 0;
		int DrawCalls = 0;
		int Uploads = 0;
		int RectUploads = 0;
	} Stats;

	int GetSettingsMultisample()
	{
		return 0;
		switch (AntialiasMode)
		{
		default:
		case 0: return 0;
		case 1: return 2;
		case 2: return 4;
		}
	}

private:
	UBOOL UsePrecache;
	FPlane FlashScale;
	FPlane FlashFog;
	FSceneNode* CurrentFrame = nullptr;
	float Aspect;
	float RProjZ;
	float RFX2;
	float RFY2;

	bool IsLocked = false;

	void SetPipeline(VulkanPipeline* pipeline);
	void DrawBatch(VulkanCommandBuffer* cmdbuffer);
	void SubmitAndWait(bool present, int presentWidth, int presentHeight, bool presentFullscreen);

	vec4 ApplyInverseGamma(vec4 color);

	struct
	{
		size_t SceneIndexStart = 0;
		VulkanPipeline* Pipeline = nullptr;
	} Batch;

	ScenePushConstants pushconstants;

	size_t SceneVertexPos = 0;
	size_t SceneIndexPos = 0;

	struct PerFrame {
		StagedUpload<Object> object_upload;
		std::unique_ptr<VulkanCommandBuffer> objectUploadCommands;
	};

	struct LastScene
	{
		ULevel* level;
		std::unique_ptr<VulkanBuffer> surf_buffer;
		std::unique_ptr<VulkanBuffer> wedge_buffer;
		std::unique_ptr<VulkanBuffer> vert_buffer;
		std::unique_ptr<VulkanBuffer> surf_idx_buffer;
		std::unique_ptr<VulkanBuffer> wedge_idx_buffer;
		//std::unique_ptr<VulkanBuffer> lightMapBuffer;
		std::unique_ptr<VulkanBuffer> lights_buffer;

		std::unique_ptr<VulkanBuffer> meshlet_buffer;
		std::unique_ptr<VulkanBuffer> meshlet_vertex_buffer;
		std::unique_ptr<VulkanBuffer> meshlet_vert_idx_buffer;
		std::unique_ptr<VulkanBuffer> meshlet_local_idx_buffer;
		std::unique_ptr<VulkanBuffer> meshlet_draw_commands_buffer;
		u32 num_meshlet_draw_commands;

		std::map<UModel*, ModelBase> model_bases;
		std::map<UMesh*, ModelBase> mesh_bases;
		std::map<UTexture*, u32> texture_to_idx;
		std::vector<UploadedTexture> uploaded_textures;
		int max_num_objects;

		PerFrame per_frame[2];
		bool odd_even;

		std::set<UModel*> missing_models;
		std::set<UMesh*> missing_meshes;

		std::optional<ModelBase> model_base_for_actor(const AActor* actor);
	};
	std::optional<LastScene> last_scene = std::nullopt;
};

inline void UVulkanRenderDevice::SetPipeline(VulkanPipeline* pipeline)
{
	if (pipeline != Batch.Pipeline)
	{
		DrawBatch(Commands->GetDrawCommands());
		Batch.Pipeline = pipeline;
	}
}

inline float GetUMult(const FTextureInfo& Info) { return 1.0f / (Info.UScale * Info.USize); }
inline float GetVMult(const FTextureInfo& Info) { return 1.0f / (Info.VScale * Info.VSize); }
