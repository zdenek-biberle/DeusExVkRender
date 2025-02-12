#pragma once

class UVulkanRenderDevice;

class SamplerManager
{
public:
	SamplerManager(UVulkanRenderDevice* renderer);

	std::unique_ptr<VulkanSampler> Samplers[4];

private:
	UVulkanRenderDevice* renderer = nullptr;
};
