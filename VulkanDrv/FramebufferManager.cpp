
#include "Precomp.h"
#include "FramebufferManager.h"
#include "UVulkanRenderDevice.h"

FramebufferManager::FramebufferManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
}

void FramebufferManager::CreateSwapChainFramebuffers()
{
	renderer->RenderPasses->CreateRenderPass();
	renderer->RenderPasses->CreatePipelines();

	auto swapchain = renderer->Commands->SwapChain.get();
	for (int i = 0; i < swapchain->ImageCount(); i++)
	{
		SwapChainFramebuffers.push_back(
			FramebufferBuilder()
				.RenderPass(renderer->RenderPasses->Scene.RenderPass.get())
				.Size(renderer->Textures->Scene->Width, renderer->Textures->Scene->Height)
				.AddAttachment(swapchain->GetImageView(i))
				.AddAttachment(renderer->Textures->Scene->DepthBufferView.get())
				.DebugName("SwapChainFramebuffer")
				.Create(renderer->Device.get()));
	}
}

void FramebufferManager::DestroySwapChainFramebuffers()
{
	SwapChainFramebuffers.clear();
}

VulkanFramebuffer* FramebufferManager::GetSwapChainFramebuffer()
{
	return SwapChainFramebuffers[renderer->Commands->PresentImageIndex].get();
}
