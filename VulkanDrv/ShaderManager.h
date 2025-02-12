#pragma once

#include "mat.h"

class UVulkanRenderDevice;

struct SceneVertex
{
	uint32_t Flags;
	vec3 Position;
	vec2 TexCoord;
	vec2 TexCoord2;
	vec2 TexCoord3;
	vec2 TexCoord4;
	vec4 Color;
	ivec4 TextureBinds;
};

struct ScenePushConstants
{
	mat4 objectToProjection;
	vec4 nearClip;
	uint32_t hitIndex;
	uint32_t padding1, padding2, padding3;
};

struct NewScenePushConstants
{
	mat4 objectToProjection;
};

class ShaderManager
{
public:
	ShaderManager(UVulkanRenderDevice* renderer);
	~ShaderManager();

	struct SceneShaders
	{
		std::unique_ptr<VulkanShader> VertexShader;
		std::unique_ptr<VulkanShader> FragmentShader;
		std::unique_ptr<VulkanShader> FragmentShaderAlphaTest;
	} SceneBindless;

	struct NewSceneShaders
	{
		std::unique_ptr<VulkanShader> VertexShader;
		std::unique_ptr<VulkanShader> FragmentShader;
	} NewScene;

	static std::string LoadShaderCode(const std::string& filename, const std::string& defines = {});

private:
	UVulkanRenderDevice* renderer = nullptr;
};
