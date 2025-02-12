#pragma once

#include "SceneTextures.h"

class UVulkanRenderDevice;

class FramebufferManager
{
public:
	FramebufferManager(UVulkanRenderDevice* renderer);

	void CreateSwapChainFramebuffers();
	void DestroySwapChainFramebuffers();

	VulkanFramebuffer* GetSwapChainFramebuffer();

private:
	UVulkanRenderDevice* renderer = nullptr;
	std::vector<std::unique_ptr<VulkanFramebuffer>> SwapChainFramebuffers;
};
