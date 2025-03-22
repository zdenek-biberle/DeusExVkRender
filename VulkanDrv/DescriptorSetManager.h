#pragma once

#include "SceneTextures.h"
#include <unordered_map>

class UVulkanRenderDevice;
class CachedTexture;

struct TexDescriptorKey
{
	TexDescriptorKey(CachedTexture* tex, CachedTexture* lightmap, CachedTexture* detailtex, CachedTexture* macrotex, uint32_t sampler) : tex(tex), lightmap(lightmap), detailtex(detailtex), macrotex(macrotex), sampler(sampler) { }

	bool operator==(const TexDescriptorKey& other) const
	{
		return tex == other.tex && lightmap == other.lightmap && detailtex == other.detailtex && macrotex == other.macrotex && sampler == other.sampler;
	}

	bool operator<(const TexDescriptorKey& other) const
	{
		if (tex != other.tex)
			return tex < other.tex;
		else if (lightmap != other.lightmap)
			return lightmap < other.lightmap;
		else if (detailtex != other.detailtex)
			return detailtex < other.detailtex;
		else if (macrotex != other.macrotex)
			return macrotex < other.macrotex;
		else
			return sampler < other.sampler;
	}

	CachedTexture* tex;
	CachedTexture* lightmap;
	CachedTexture* detailtex;
	CachedTexture* macrotex;
	uint32_t sampler;
};

template<> struct std::hash<TexDescriptorKey>
{
	std::size_t operator()(const TexDescriptorKey& k) const
	{
		return (((std::size_t)k.tex ^ (std::size_t)k.lightmap));
	}
};

class DescriptorSetManager
{
public:
	DescriptorSetManager(UVulkanRenderDevice* renderer);
	~DescriptorSetManager();

	static const int MaxBindlessTextures = 16536;

	VulkanDescriptorSetLayout* GetNewLayout() { return Textures.NewLayout.get(); }
	VulkanDescriptorSetLayout* GetMeshLayout() { return Textures.MeshLayout.get(); }
	VulkanDescriptorSet* GetNewSet(bool odd_even) { return Textures.NewSet[odd_even].get(); }
	VulkanDescriptorSet* GetMeshletSet(bool odd_even) { return Textures.MeshletSet[odd_even].get(); }

private:
	void CreateBindlessTextureSet();

	UVulkanRenderDevice* renderer = nullptr;

	struct
	{
		std::unique_ptr<VulkanDescriptorPool> NewPool;
		std::unique_ptr<VulkanDescriptorSetLayout> NewLayout;
		std::unique_ptr<VulkanDescriptorSet> NewSet[2];
		std::unique_ptr<VulkanDescriptorSetLayout> MeshLayout;
		std::unique_ptr<VulkanDescriptorSet> MeshletSet[2];
	} Textures;
};
