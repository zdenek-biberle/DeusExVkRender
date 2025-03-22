
#include "Precomp.h"
#include "ShaderManager.h"
#include "FileResource.h"
#include "UVulkanRenderDevice.h"

ShaderManager::ShaderManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	ShaderBuilder::Init();

	guard(ShaderManager::ShaderManager::vert);
	NewScene.VertexShader = ShaderBuilder()
		.Type(ShaderType::Vertex)
		.AddSource("scene.vert", readShader(IDR_SCENE_VERT))
		.DebugName("newVertexShader")
		.Create("newVertexShader", renderer->Device.get());
	unguard;

	guard(ShaderManager::ShaderManager::frag);
	NewScene.FragmentShader = ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource("scene.frag", readShader(IDR_SCENE_FRAG))
		.DebugName("newFragmentShader")
		.Create("newFragmentShader", renderer->Device.get());
	unguard;

	guard(ShaderManager::ShaderManager::mesh_vert);
	MeshScene.VertexShader = ShaderBuilder()
		.Type(ShaderType::Vertex)
		.AddSource("scene-mesh.vert", readShader(IDR_SCENE_MESH_VERT))
		.DebugName("meshVertexShader")
		.Create("meshVertexShader", renderer->Device.get());
	unguard;

	guard(ShaderManager::ShaderManager::mesh_frag);
	MeshScene.FragmentShader = ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource("scene-mesh.frag", readShader(IDR_SCENE_MESH_FRAG))
		.DebugName("meshFragmentShader")
		.Create("meshFragmentShader", renderer->Device.get());
	unguard;

	SceneBindless.VertexShader = ShaderBuilder()
		.Type(ShaderType::Vertex)
		.AddSource("shaders/Scene.vert", LoadShaderCode("shaders/Scene.vert", "#extension GL_EXT_nonuniform_qualifier : enable\r\n#define BINDLESS_TEXTURES"))
		.DebugName("vertexShader")
		.Create("vertexShader", renderer->Device.get());

	SceneBindless.FragmentShader = ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource("shaders/Scene.frag", LoadShaderCode("shaders/Scene.frag", "#extension GL_EXT_nonuniform_qualifier : enable\r\n#define BINDLESS_TEXTURES"))
		.DebugName("fragmentShader")
		.Create("fragmentShader", renderer->Device.get());

	SceneBindless.FragmentShaderAlphaTest = ShaderBuilder()
		.Type(ShaderType::Fragment)
		.AddSource("shaders/Scene.frag", LoadShaderCode("shaders/Scene.frag", "#extension GL_EXT_nonuniform_qualifier : enable\r\n#define BINDLESS_TEXTURES\r\n#define ALPHATEST"))
		.DebugName("fragmentShader")
		.Create("fragmentShader", renderer->Device.get());
}

ShaderManager::~ShaderManager()
{
	ShaderBuilder::Deinit();
}

std::string ShaderManager::LoadShaderCode(const std::string& filename, const std::string& defines)
{
	const char* shaderversion = R"(
		#version 450
		#extension GL_ARB_separate_shader_objects : enable
	)";
	return shaderversion + defines + "\r\n#line 1\r\n" + FileResource::readAllText(filename);
}
