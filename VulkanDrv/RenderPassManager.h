#pragma once

class UVulkanRenderDevice;

class RenderPassManager
{
public:
	RenderPassManager(UVulkanRenderDevice* renderer);
	~RenderPassManager();

	void CreateRenderPass();
	void CreatePipelines();

	void BeginScene(VulkanCommandBuffer* cmdbuffer, float r, float g, float b, float a);
	void EndScene(VulkanCommandBuffer* cmdbuffer);

	VulkanPipeline* GetPipeline(DWORD polyflags);
	VulkanPipeline* GetEndFlashPipeline();
	VulkanPipeline* GetLinePipeline(bool occludeLines) { return Scene.LinePipeline[occludeLines].get(); }
	VulkanPipeline* GetPointPipeline(bool occludeLines) { return Scene.PointPipeline[occludeLines].get(); }

	struct
	{
		std::unique_ptr<VulkanPipelineLayout> BindlessPipelineLayout;
		std::unique_ptr<VulkanRenderPass> RenderPass;
		std::unique_ptr<VulkanPipeline> Pipeline[32];
		std::unique_ptr<VulkanPipeline> LinePipeline[2];
		std::unique_ptr<VulkanPipeline> PointPipeline[2];

		std::unique_ptr<VulkanPipelineLayout> NewPipelineLayout;
		std::unique_ptr<VulkanPipeline> NewPipeline;
	} Scene;

private:
	void CreateSceneBindlessPipelineLayout();

	UVulkanRenderDevice* renderer = nullptr;
};
