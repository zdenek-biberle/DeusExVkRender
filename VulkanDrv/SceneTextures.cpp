
#include "Precomp.h"
#include "SceneTextures.h"
#include "UVulkanRenderDevice.h"

SceneTextures::SceneTextures(UVulkanRenderDevice* renderer, int width, int height, int multisample) : Width(width), Height(height), Multisample(multisample)
{
	SceneSamples = GetBestSampleCount(renderer->Device.get(), multisample);

	DepthBuffer = ImageBuilder()
		.Size(width, height)
		.Samples(SceneSamples)
		.Format(VK_FORMAT_D32_SFLOAT)
		.Usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		.DebugName("depthBuffer")
		.Create(renderer->Device.get());

	DepthBufferView = ImageViewBuilder()
		.Image(DepthBuffer.get(), VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT)
		.DebugName("depthBufferView")
		.Create(renderer->Device.get());

	PipelineBarrier barrier;

	barrier.AddImage(
		DepthBuffer.get(),
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		0,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT);

	barrier.Execute(
		renderer->Commands->GetDrawCommands(),
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
}

SceneTextures::~SceneTextures()
{
}

VkSampleCountFlagBits SceneTextures::GetBestSampleCount(VulkanDevice* device, int multisample)
{
	return VK_SAMPLE_COUNT_1_BIT;
}
