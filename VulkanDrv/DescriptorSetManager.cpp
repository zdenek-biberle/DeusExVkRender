
#include "Precomp.h"
#include "DescriptorSetManager.h"
#include "UVulkanRenderDevice.h"
#include "CachedTexture.h"

DescriptorSetManager::DescriptorSetManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	debugf(TEXT("CreateBindlessTextureSet"));
	CreateBindlessTextureSet();
}

DescriptorSetManager::~DescriptorSetManager()
{
}

void DescriptorSetManager::CreateBindlessTextureSet()
{
	Textures.NewPool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7 * 2 + 4 * 2)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 1 * 2 + 1 * 2)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MaxBindlessTextures * 2 + MaxBindlessTextures * 2)
		.MaxSets(4)
		.DebugName("NewPool")
		.Create(renderer->Device.get());

	Textures.NewLayout = DescriptorSetLayoutBuilder()
		// surf buffer
		.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		// wedge buffer
		.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		// vert buffer
		.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		// object buffer
		.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		// surf index buffer
		.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		// vert index buffer
		.AddBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		// light buffer
		.AddBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		// sampler
		.AddBinding(7, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		// textures
		.AddBinding(8, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MaxBindlessTextures, VK_SHADER_STAGE_FRAGMENT_BIT,
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT)
		.DebugName("NewLayout")
		.Create(renderer->Device.get());

	Textures.NewSet[false] = Textures.NewPool->allocate(Textures.NewLayout.get(), MaxBindlessTextures);
	Textures.NewSet[true] = Textures.NewPool->allocate(Textures.NewLayout.get(), MaxBindlessTextures);

	Textures.MeshLayout = DescriptorSetLayoutBuilder()
		// meshlet buffer
		.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		// vertex buffer
		.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		// meshlet vertex index buffer
		.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		// meshlet local index buffer
		.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
		// sampler
		.AddBinding(4, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		// textures
		.AddBinding(5, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MaxBindlessTextures, VK_SHADER_STAGE_FRAGMENT_BIT,
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT)
		.DebugName("MeshLayout")
		.Create(renderer->Device.get());

	Textures.MeshletSet[false] = Textures.NewPool->allocate(Textures.MeshLayout.get(), MaxBindlessTextures);
	Textures.MeshletSet[true] = Textures.NewPool->allocate(Textures.MeshLayout.get(), MaxBindlessTextures);
}
