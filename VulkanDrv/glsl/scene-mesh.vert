#version 450
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_8bit_storage : enable

struct Vertex {
	float x, y, z;
	float nx, ny, nz;
	float u, v;
};

struct Meshlet {
	// where in the verts array does this meshlet start?
	uint vertOffset;
	// how many verts in the verts array does this meshlet use?
	uint vertCount;
	// where in the meshletLocalIndices array does this meshlet start?
	// this is the index of the first byte, not the index of the first
	// uint
	uint localOffset;
	// how many triangles does this meshlet have? each meshlet
	// will consume triCount * 3 indices from the meshletLocalIndices array
	uint triCount;
	uint texIdx;
};

layout(push_constant) uniform ScenePushConstants
{
	mat4 objectToProjection;
};

layout(std430, binding = 0) readonly buffer MeshletBuffer{ Meshlet meshlets[]; };
layout(std430, binding = 1) readonly buffer VertBuffer{ Vertex verts[]; };
layout(std430, binding = 2) readonly buffer MeshletVertIndexBuffer{ uint meshletVertIndices[]; };
layout(std430, binding = 3) readonly buffer MeshletLocalIndexBuffer{ uint8_t meshletLocalIndices[]; };
// TODO: objects?

layout(location = 0) out vec3 outNormal;
layout(location = 1) flat out uint outTexIndex;
layout(location = 2) out vec2 outTexCoord;

void main()
{
	Meshlet meshlet = meshlets[gl_InstanceIndex];
	uint localIdx = uint(meshletLocalIndices[gl_VertexIndex]);
	uint vertIdx = meshletVertIndices[meshlet.vertOffset + localIdx];
	Vertex vert = verts[vertIdx];

	vec3 point = vec3(vert.x, vert.y, vert.z);
	vec3 normal = vec3(vert.nx, vert.ny, vert.nz);
	vec2 uv = vec2(vert.u, vert.v);

	outNormal = normal;
	outTexIndex = meshlet.texIdx;
	outTexCoord = uv;

	gl_Position = objectToProjection * vec4(point, 1.0);
}