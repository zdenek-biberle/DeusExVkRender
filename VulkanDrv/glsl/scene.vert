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
	uint vertOffset1;
	uint vertOffset2;
	float vertLerp;
	float white;
	uint pad[4];
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
layout(location = 3) out float outWhite;

void main()
{
	Object obj = objects[gl_InstanceIndex];

	uint surfIdx = surfIndices[gl_VertexIndex / 3];
	uint wedgeIdx = wedgeIndices[gl_VertexIndex];
	
	Surf surf = surfs[surfIdx];
	Wedge wedge = wedges[wedgeIdx];
	Vertex vert1 = verts[wedge.vertIdx + obj.vertOffset1];
	Vertex vert2 = verts[wedge.vertIdx + obj.vertOffset2];

	vec3 point1 = vec3(vert1.x, vert1.y, vert1.z);
	vec3 point2 = vec3(vert2.x, vert2.y, vert2.z);
	vec3 point = mix(point1, point2, obj.vertLerp);
	vec2 uv = vec2(wedge.u, wedge.v);
	vec3 normal = vec3(surf.normalX, surf.normalY, surf.normalZ);

	outNormal = normal;
	outTexIndex = surf.texIdx < 0 ? obj.textures[-surf.texIdx - 1] : surf.texIdx;
	outTexCoord = uv;
	outWhite = obj.white;
	gl_Position = objectToProjection * obj.xform * vec4(point, 1.0);
	//gl_ClipDistance[0] = dot(nearClip, vec4(aPosition, 1.0));
}