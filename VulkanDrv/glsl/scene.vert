#version 450
#extension GL_EXT_scalar_block_layout : enable

struct Surf {
	float normalX, normalY, normalZ;
	int texIdx;
	uint lightsIdx;
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
	uint pad[5];
};

//struct LightMapIndex {
//	vec3 pan;
//	int texIndex;
//	float uscale, vscale;
//	uint pad[2];
//};

struct Light {
	vec3 position;
	int isLight;
	vec3 color;
	int pad;
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
//layout(std430, binding = 6) readonly buffer LightMapIndexBuffer{ LightMapIndex lightMapIndices[]; };

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outWorldPosition;
layout(location = 2) flat out uint outTexIndex;
layout(location = 3) out vec2 outTexCoord;
layout(location = 4) flat out uint outLightsIndex;

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
	outLightsIndex = surf.lightsIdx;
//	if (surf.lightMapTexIdx != ~0u) {
//		LightMapIndex lightMapIndex = lightMapIndices[surf.lightMapTexIdx];
//		outLightMapIndex = lightMapIndex.texIndex;
//	} else {
//		outLightMapIndex = ~0u;
//	}
	vec4 worldPosition = obj.xform * vec4(point, 1.0);
	outWorldPosition = worldPosition.xyz;
	gl_Position = objectToProjection * worldPosition;
	//gl_ClipDistance[0] = dot(nearClip, vec4(aPosition, 1.0));
}