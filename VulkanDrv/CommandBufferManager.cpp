
#include "Precomp.h"
#include "CommandBufferManager.h"
#include "UVulkanRenderDevice.h"

CommandBufferManager::CommandBufferManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	SwapChain = VulkanSwapChainBuilder()
		.Create(renderer->Device.get());

	ImageAvailableSemaphore = SemaphoreBuilder()
		.DebugName("ImageAvailableSemaphore")
		.Create(renderer->Device.get());

	RenderFinishedSemaphore = SemaphoreBuilder()
		.DebugName("RenderFinishedSemaphore")
		.Create(renderer->Device.get());

	RenderFinishedFence = FenceBuilder()
		.DebugName("RenderFinishedFence")
		.Create(renderer->Device.get());

	TransferSemaphore.reset(new VulkanSemaphore(renderer->Device.get()));

	CommandPool = CommandPoolBuilder()
		.QueueFamily(renderer->Device.get()->GraphicsFamily)
		.DebugName("CommandPool")
		.Create(renderer->Device.get());

	FrameDeleteList = std::make_unique<DeleteList>();
}

CommandBufferManager::~CommandBufferManager()
{
	DeleteFrameObjects();
}

void CommandBufferManager::SubmitCommands(bool present, int presentWidth, int presentHeight, bool presentFullscreen)
{
	if (DrawCommands)
		DrawCommands->end();

	QueueSubmit submit;
	if (DrawCommands)
	{
		submit.AddCommandBuffer(DrawCommands.get());
	}
	//if (TransferCommands)
	//{
	//	submit.AddWait(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TransferSemaphore.get());
	//}
	if (present && PresentImageIndex != -1)
	{
		submit.AddWait(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, ImageAvailableSemaphore.get());
		submit.AddSignal(RenderFinishedSemaphore.get());
	}
	submit.Execute(renderer->Device.get(), renderer->Device.get()->GraphicsQueue, RenderFinishedFence.get());

	if (present && PresentImageIndex != -1)
	{
		SwapChain->QueuePresent(PresentImageIndex, RenderFinishedSemaphore.get());
	}

	vkWaitForFences(renderer->Device.get()->device, 1, &RenderFinishedFence->fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(renderer->Device.get()->device, 1, &RenderFinishedFence->fence);

	DrawCommands.reset();
	//TransferCommands.reset();
	DeleteFrameObjects();
}

VulkanCommandBuffer* CommandBufferManager::GetDrawCommands()
{
	if (!DrawCommands)
	{
		DrawCommands = CommandPool->createBuffer();
		DrawCommands->begin();
	}
	return DrawCommands.get();
}

void CommandBufferManager::DeleteFrameObjects()
{
	FrameDeleteList = std::make_unique<DeleteList>();
}

void CommandBufferManager::AcquirePresentImage(int presentWidth, int presentHeight, bool presentFullscreen)
{
	if (SwapChain->Lost() || SwapChain->Width() != presentWidth || SwapChain->Height() != presentHeight || UsingVsync != renderer->UseVSync || UsingHdr != renderer->Hdr)
	{
		try {
			UsingVsync = renderer->UseVSync;
			UsingHdr = renderer->Hdr;
			renderer->Framebuffers->DestroySwapChainFramebuffers();
			SwapChain->Create(presentWidth, presentHeight, renderer->UseVSync ? 2 : 3, renderer->UseVSync, renderer->Hdr, renderer->VkExclusiveFullscreen && presentFullscreen);
			renderer->Framebuffers->CreateSwapChainFramebuffers();
		}
		catch (...) {
			debugf(L"Oopsie recreating swapchain");
		}
	}

	PresentImageIndex = SwapChain->AcquireImage(ImageAvailableSemaphore.get());
}

std::unique_ptr<VulkanCommandBuffer> CommandBufferManager::CreateCommandBuffer()
{
	return CommandPool->createBuffer();
}