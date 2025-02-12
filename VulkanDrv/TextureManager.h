#pragma once

#include "SceneTextures.h"

struct FTextureInfo;
class UVulkanRenderDevice;
class CachedTexture;

class TextureManager
{
public:
	TextureManager(UVulkanRenderDevice* renderer);
	~TextureManager();

	std::unique_ptr<SceneTextures> Scene;

private:
	UVulkanRenderDevice* renderer = nullptr;
};
