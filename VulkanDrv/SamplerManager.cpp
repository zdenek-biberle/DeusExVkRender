
#include "Precomp.h"
#include "SamplerManager.h"
#include "UVulkanRenderDevice.h"

SamplerManager::SamplerManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	for (int i = 0; i < 4; i++)
	{
		SamplerBuilder builder;
		builder.Anisotropy(8.0f);
		builder.MipLodBias(renderer->LODBias);

		if (i & 1)
		{
			builder.MinFilter(VK_FILTER_NEAREST);
			builder.MagFilter(VK_FILTER_NEAREST);
			builder.MipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
		}
		else
		{
			builder.MinFilter(VK_FILTER_LINEAR);
			builder.MagFilter(VK_FILTER_LINEAR);
			builder.MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR);
		}

		if (i & 2)
		{
			builder.AddressMode(VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE);
		}
		else
		{
			builder.AddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT);
		}

		builder.DebugName("SceneSampler");

		Samplers[i] = builder.Create(renderer->Device.get());
	}
}
