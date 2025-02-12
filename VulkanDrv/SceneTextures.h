#pragma once

#include "ShaderManager.h"
#include "mat.h"
#include "vec.h"

class UVulkanRenderDevice;

enum { NumBloomLevels = 4 };

class SceneTextures
{
public:
	SceneTextures(UVulkanRenderDevice* renderer, int width, int height, int multisample);
	~SceneTextures();

	// Current active multisample setting
	VkSampleCountFlagBits SceneSamples = VK_SAMPLE_COUNT_1_BIT;

	// Scene framebuffer depth buffer
	std::unique_ptr<VulkanImage> DepthBuffer;
	std::unique_ptr<VulkanImageView> DepthBufferView;

	// Size of the scene framebuffer
	int Width = 0;
	int Height = 0;
	int Multisample = 0;

private:
	static VkSampleCountFlagBits GetBestSampleCount(VulkanDevice* device, int multisample);
};
