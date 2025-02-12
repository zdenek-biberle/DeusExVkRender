
#include "Precomp.h"
#include "RenderPassManager.h"
#include "UVulkanRenderDevice.h"

RenderPassManager::RenderPassManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	CreateSceneBindlessPipelineLayout();
}

RenderPassManager::~RenderPassManager()
{
}

void RenderPassManager::CreateSceneBindlessPipelineLayout()
{
	Scene.BindlessPipelineLayout = PipelineLayoutBuilder()
		//.AddSetLayout(renderer->DescriptorSets->GetTextureBindlessLayout())
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants))
		.DebugName("SceneBindlessPipelineLayout")
		.Create(renderer->Device.get());

	Scene.NewPipelineLayout = PipelineLayoutBuilder()
		.AddSetLayout(renderer->DescriptorSets->GetNewLayout())
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(NewScenePushConstants))
		.DebugName("NewScenePipelineLayout")
		.Create(renderer->Device.get());
}

void RenderPassManager::BeginScene(VulkanCommandBuffer* cmdbuffer, float r, float g, float b, float a)
{
	RenderPassBegin()
		.RenderPass(Scene.RenderPass.get())
		//.Framebuffer(renderer->Framebuffers->SceneFramebuffer.get())
		.Framebuffer(renderer->Framebuffers->GetSwapChainFramebuffer())
		.RenderArea(0, 0, renderer->Textures->Scene->Width, renderer->Textures->Scene->Height)
		.AddClearColor(r, g, b, a)
		.AddClearDepthStencil(1.0f, 0)
		.Execute(cmdbuffer);
}

void RenderPassManager::EndScene(VulkanCommandBuffer* cmdbuffer)
{
	cmdbuffer->endRenderPass();
}

VulkanPipeline* RenderPassManager::GetPipeline(DWORD PolyFlags)
{
	// Adjust PolyFlags according to Unreal's precedence rules.
	if (!(PolyFlags & (PF_Translucent | PF_Modulated)))
		PolyFlags |= PF_Occlude;
	else if (PolyFlags & PF_Translucent)
		PolyFlags &= ~PF_Masked;

	int index;
	if (PolyFlags & PF_Translucent)
	{
		index = 0;
	}
	else if (PolyFlags & PF_Modulated)
	{
		index = 1;
	}
	else if (PolyFlags & PF_Highlighted)
	{
		index = 2;
	}
	else
	{
		index = 3;
	}

	if (PolyFlags & PF_Invisible)
	{
		index |= 4;
	}
	if (PolyFlags & PF_Occlude)
	{
		index |= 8;
	}
	if (PolyFlags & PF_Masked)
	{
		index |= 16;
	}

	return Scene.Pipeline[index].get();
}

VulkanPipeline* RenderPassManager::GetEndFlashPipeline()
{
	return Scene.Pipeline[2].get();
}

void RenderPassManager::CreatePipelines()
{
	VulkanShader* vertShader = renderer->Shaders->SceneBindless.VertexShader.get();
	VulkanShader* fragShader = renderer->Shaders->SceneBindless.FragmentShader.get();
	VulkanShader* fragShaderAlphaTest = renderer->Shaders->SceneBindless.FragmentShaderAlphaTest.get();
	VulkanPipelineLayout* layout = Scene.BindlessPipelineLayout.get();

	for (int type = 1; type < 2; type++)
	{
		for (int i = 0; i < 32; i++)
		{
			GraphicsPipelineBuilder builder;
			builder.AddVertexShader(vertShader);
			builder.Viewport(0.0f, 0.0f, (float)renderer->Textures->Scene->Width, (float)renderer->Textures->Scene->Height);
			builder.Scissor(0, 0, renderer->Textures->Scene->Width, renderer->Textures->Scene->Height);
			builder.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
			builder.Cull(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
			builder.AddVertexBufferBinding(0, sizeof(SceneVertex));
			builder.AddVertexAttribute(0, 0, VK_FORMAT_R32_UINT, offsetof(SceneVertex, Flags));
			builder.AddVertexAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneVertex, Position));
			builder.AddVertexAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord));
			builder.AddVertexAttribute(3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord2));
			builder.AddVertexAttribute(4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord3));
			builder.AddVertexAttribute(5, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord4));
			builder.AddVertexAttribute(6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SceneVertex, Color));
			if (type == 1)
				builder.AddVertexAttribute(7, 0, VK_FORMAT_R32G32B32A32_SINT, offsetof(SceneVertex, TextureBinds));
			builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
			builder.Layout(layout);
			builder.RenderPass(Scene.RenderPass.get());

			// Avoid clipping the weapon. The UE1 engine clips the geometry anyway.
			if (renderer->Device.get()->EnabledFeatures.Features.depthClamp)
				builder.DepthClampEnable(true);

			ColorBlendAttachmentBuilder colorblend;
			switch (i & 3)
			{
			case 0: // PF_Translucent
				colorblend.BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR);
				builder.DepthBias(true, -1.0f, 0.0f, -1.0f);
				break;
			case 1: // PF_Modulated
				colorblend.BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_SRC_COLOR);
				builder.DepthBias(true, -1.0f, 0.0f, -1.0f);
				break;
			case 2: // PF_Highlighted
				colorblend.BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
				builder.DepthBias(true, -1.0f, 0.0f, -1.0f);
				break;
			case 3:
				colorblend.BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO); // Hmm, is it faster to keep the blend mode enabled or to toggle it?
				break;
			}

			if (i & 4) // PF_Invisible
			{
				colorblend.ColorWriteMask(0);
			}

			if (i & 8) // PF_Occlude
			{
				builder.DepthStencilEnable(true, true, false);
			}
			else
			{
				builder.DepthStencilEnable(true, false, false);
			}

			if (i & 16) // PF_Masked
				builder.AddFragmentShader(fragShaderAlphaTest);
			else
				builder.AddFragmentShader(fragShader);

			builder.AddColorBlendAttachment(colorblend.Create());
			//builder.AddColorBlendAttachment(ColorBlendAttachmentBuilder().Create());

			builder.RasterizationSamples(renderer->Textures->Scene->SceneSamples);
			builder.DebugName("SceneBindlessPipeline");

			try {
				Scene.Pipeline[i] = builder.Create(renderer->Device.get());
			}
			catch (...) {
				debugf(L"Oopsie - scene pipeline");
			}
		}

		// Line pipeline
		for (int i = 0; i < 2; i++)
		{
			GraphicsPipelineBuilder builder;
			builder.AddVertexShader(vertShader);
			builder.Viewport(0.0f, 0.0f, (float)renderer->Textures->Scene->Width, (float)renderer->Textures->Scene->Height);
			builder.Scissor(0, 0, renderer->Textures->Scene->Width, renderer->Textures->Scene->Height);
			builder.Topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
			builder.Cull(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
			builder.AddVertexBufferBinding(0, sizeof(SceneVertex));
			builder.AddVertexAttribute(0, 0, VK_FORMAT_R32_UINT, offsetof(SceneVertex, Flags));
			builder.AddVertexAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneVertex, Position));
			builder.AddVertexAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord));
			builder.AddVertexAttribute(3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord2));
			builder.AddVertexAttribute(4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord3));
			builder.AddVertexAttribute(5, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord4));
			builder.AddVertexAttribute(6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SceneVertex, Color));
			if (type == 1)
				builder.AddVertexAttribute(7, 0, VK_FORMAT_R32G32B32A32_SINT, offsetof(SceneVertex, TextureBinds));
			builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
			builder.Layout(layout);
			builder.RenderPass(Scene.RenderPass.get());

			builder.AddColorBlendAttachment(ColorBlendAttachmentBuilder().BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA).Create());
			//builder.AddColorBlendAttachment(ColorBlendAttachmentBuilder().Create());

			builder.DepthStencilEnable(i == 1, false, false);
			builder.AddFragmentShader(fragShader);

			builder.RasterizationSamples(renderer->Textures->Scene->SceneSamples);
			builder.DebugName("LinePipeline");

			try {
				Scene.LinePipeline[i] = builder.Create(renderer->Device.get());
			}
			catch (...) {
				debugf(L"Oopsie - line pipeline");
			}
		}

		// Point pipeline
		for (int i = 0; i < 2; i++)
		{
			GraphicsPipelineBuilder builder;
			builder.AddVertexShader(vertShader);
			builder.AddFragmentShader(fragShader);
			builder.Viewport(0.0f, 0.0f, (float)renderer->Textures->Scene->Width, (float)renderer->Textures->Scene->Height);
			builder.Scissor(0, 0, renderer->Textures->Scene->Width, renderer->Textures->Scene->Height);
			builder.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
			builder.Cull(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
			builder.AddVertexBufferBinding(0, sizeof(SceneVertex));
			builder.AddVertexAttribute(0, 0, VK_FORMAT_R32_UINT, offsetof(SceneVertex, Flags));
			builder.AddVertexAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneVertex, Position));
			builder.AddVertexAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord));
			builder.AddVertexAttribute(3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord2));
			builder.AddVertexAttribute(4, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord3));
			builder.AddVertexAttribute(5, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SceneVertex, TexCoord4));
			builder.AddVertexAttribute(6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SceneVertex, Color));
			if (type == 1)
				builder.AddVertexAttribute(7, 0, VK_FORMAT_R32G32B32A32_SINT, offsetof(SceneVertex, TextureBinds));
			builder.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
			builder.Layout(layout);
			builder.RenderPass(Scene.RenderPass.get());

			builder.AddColorBlendAttachment(ColorBlendAttachmentBuilder().BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA).Create());
			//builder.AddColorBlendAttachment(ColorBlendAttachmentBuilder().Create());

			builder.DepthStencilEnable(false, false, false);
			builder.RasterizationSamples(renderer->Textures->Scene->SceneSamples);
			builder.DebugName("PointPipeline");

			try {
				Scene.PointPipeline[i] = builder.Create(renderer->Device.get());
			}
			catch (...) {
				debugf(L"Oopsie");
			}
		}
	}

	Scene.NewPipeline = GraphicsPipelineBuilder()
		.AddVertexShader(renderer->Shaders->NewScene.VertexShader.get())
		.AddFragmentShader(renderer->Shaders->NewScene.FragmentShader.get())
		.Viewport(0.0f, 0.0f, (float)renderer->Textures->Scene->Width, (float)renderer->Textures->Scene->Height)
		.Scissor(0, 0, renderer->Textures->Scene->Width, renderer->Textures->Scene->Height)
		.Topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.Cull(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.Layout(Scene.NewPipelineLayout.get())
		.RenderPass(Scene.RenderPass.get())
		.AddColorBlendAttachment(ColorBlendAttachmentBuilder().BlendMode(VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA).Create())
		//.AddColorBlendAttachment(ColorBlendAttachmentBuilder().Create())
		.DepthStencilEnable(true, true, false)
		.Create(renderer->Device.get());
}

void RenderPassManager::CreateRenderPass()
{
	Scene.RenderPass = RenderPassBuilder()
		.AddAttachment(
			renderer->Commands->SwapChain->Format().format,
			renderer->Textures->Scene->SceneSamples,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		//.AddAttachment(
		//	VK_FORMAT_R32_UINT,
		//	renderer->Textures->Scene->SceneSamples,
		//	VK_ATTACHMENT_LOAD_OP_CLEAR,
		//	VK_ATTACHMENT_STORE_OP_STORE,
		//	VK_IMAGE_LAYOUT_UNDEFINED,
		//	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddDepthStencilAttachment(
			VK_FORMAT_D32_SFLOAT,
			renderer->Textures->Scene->SceneSamples,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		.AddExternalSubpassDependency(
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
		.AddSubpass()
		.AddSubpassColorAttachmentRef(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		//.AddSubpassColorAttachmentRef(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.AddSubpassDepthStencilAttachmentRef(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		.DebugName("SceneRenderPass")
		.Create(renderer->Device.get());
}
