#version 450
#extension GL_EXT_scalar_block_layout : enable

struct Surf {
	float normalX, normalY, normalZ;
	int texIdx;
};

struct Wedge {
	float u, v;
	uint vertIdx;
};

struct Vertex {
	float x, y, z;
};

struct Object {
	mat4 xform;
	uint textures[8];
	uint vertOffset;
	uint pad[7];
};

layout(push_constant) uniform ScenePushConstants
{
	mat4 objectToProjection;
};

layout(std430, binding = 0) readonly buffer SurfBuffer{ Surf surfs[]; };
layout(std430, binding = 1) readonly buffer WedgeBuffer{ Wedge wedges[]; };
layout(std430, binding = 2) readonly buffer VertBuffer{ Vertex verts[]; };
layout(std430, binding = 3) readonly buffer ObjectBuffer{ Object objects[]; };
layout(scalar, binding = 4) readonly buffer SurfIdxBuffer{ uint surfIndices[]; };
layout(scalar, binding = 5) readonly buffer VertIdxBuffer{ uint wedgeIndices[]; };

layout(location = 0) out vec3 outNormal;
layout(location = 1) flat out uint outTexIndex;
layout(location = 2) out vec2 outTexCoord;

void main()
{
	Object obj = objects[gl_InstanceIndex];

	uint surfIdx = surfIndices[gl_VertexIndex / 3];
	uint wedgeIdx = wedgeIndices[gl_VertexIndex];
	
	Surf surf = surfs[surfIdx];
	Wedge wedge = wedges[wedgeIdx];
	Vertex vert = verts[wedge.vertIdx + obj.vertOffset];

	vec3 point = vec3(vert.x, vert.y, vert.z);
	vec2 uv = vec2(wedge.u, wedge.v);
	vec3 normal = vec3(surf.normalX, surf.normalY, surf.normalZ);

	outNormal = normal;
	outTexIndex = surf.texIdx < 0 ? obj.textures[-surf.texIdx - 1] : surf.texIdx;
	outTexCoord = uv;
	gl_Position = objectToProjection * obj.xform * vec4(point, 1.0);
	//gl_ClipDistance[0] = dot(nearClip, vec4(aPosition, 1.0));
}