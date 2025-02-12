
#include "Precomp.h"
#include "TextureManager.h"
#include "UVulkanRenderDevice.h"
#include "CachedTexture.h"

TextureManager::TextureManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
}

TextureManager::~TextureManager()
{
}
