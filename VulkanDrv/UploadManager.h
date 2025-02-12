#pragma once

#include "TextureUploader.h"
#include <unordered_map>

class UVulkanRenderDevice;
class CachedTexture;
struct FTextureInfo;

class UploadManager
{
public:
	UploadManager(UVulkanRenderDevice* renderer);
	~UploadManager();

	bool SupportsTextureFormat(ETextureFormat Format) const;

	void UploadTexture(CachedTexture* tex, const FTextureInfo& Info, bool masked);
	void UploadTextureRect(CachedTexture* tex, const FTextureInfo& Info, int x, int y, int w, int h);

private:
	UVulkanRenderDevice* renderer = nullptr;

	int UploadBufferPos = 0;
	std::vector<CachedTexture*> PendingUploads;
};
