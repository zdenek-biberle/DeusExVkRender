#include "Precomp.h"

#ifndef DeusExVkRender_types_h
#define DeusExVkRender_types_h

#include "mat.h"

// rust got these right
using u32 = unsigned int;
using i32 = int;
using u16 = unsigned short;
using i16 = short;
using u8 = unsigned char;
using i8 = char;
using f32 = float;
using f64 = double;

struct Vertex
{
	FVector pos;
};

struct Wedge {
	f32 u, v;
	u32 vertIndex;
};

struct Surf
{
	FVector normal;
	i32 texIdx;
	u32 lightsIdx;
};

struct alignas(16) Light {
	FVector pos;
	u32 isLight; // 0 serves as a sentinel value to indicate the end of the light list
	FVector color;
	u32 pad2;
};

// GPU equivalent of FLightMapIndex
struct LightMapIndex {
	FVector pan;
	u32 texIndex;
	f32 uscale, vscale;
	i32 pad[2];
};

static_assert(sizeof(LightMapIndex) == 32, "LightMapIndex size must be 32 bytes");

struct alignas(16) Object {
	mat4 xform;
	u32 textures[8];
	u32 vertexOffset1;
	u32 vertexOffset2;
	f32 vertexLerp;
	u32 lightMapIndex; // index of a LightMapIndex
	VkDrawIndirectCommand command;
};

static_assert(sizeof(Object) == 128, "Object size must be 128 bytes");

struct MeshletVertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
};

struct Meshlet {
	u32 vert_offset;
	u32 vert_count;
	u32 local_offset;
	u32 tri_count;
	u32 tex_idx;
};

#endif