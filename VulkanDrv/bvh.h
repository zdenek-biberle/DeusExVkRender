#ifndef BVH_H
#define BVH_H

#include "Precomp.h"

// Typing UINT in all-caps is annoying :)
typedef UINT uint;

// This is some BVH stuff. We're probably going to be using BVHs in order
// to cull geometry on the GPU.
//
// Some of the things here are inspired by:
// https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/
//
// One "special" idea is that we're going to be using wide BVH nodes. The
// idea is that that's going to help us utilize multiple threads on the GPU
// to traverse the BVH - we can, for example, check 32 nodes at once with a
// single workgroup. We'll see how that works.

// Branching factor for BVH. Has to be a power of two.
constexpr uint BVH_WIDTH = 8;
constexpr uint BVH_WIDTH_LOG2 = 3;

static_assert(BVH_WIDTH > 1, "BVH_WIDTH has to be greater than 1");
static_assert((BVH_WIDTH& (BVH_WIDTH - 1)) == 0, "BVH_WIDTH has to be a power of two");
static_assert(BVH_WIDTH == 1 << BVH_WIDTH_LOG2, "BVH_WIDTH_LOG2 has to be the log2 of BVH_WIDTH");

// Special index value for "none".
constexpr uint BVH_INDEX_NONE = ~0u;

// BVH node
struct alignas(32) BvhNode {
	// bounding box
	FVector min;
	FVector max;

	// When this is an internal node, this is the index of the first child,
	// subsequent children are stored in a contiguous block.
	// When this is a leaf node, then... it's not anything yet. Probably gonna
	// be an index of a triangle, surf, or something like that.
	uint firstChildIdx;
	
	// In the future this will be a number of triangles or something like that.
	// Right now it's just a leaf flag.
	uint leaf;

	bool isEmpty() const;
	float area() const;
	float volume() const;
};

static_assert(sizeof(BvhNode) == 32, "BvhNode is supposed to be 32 bytes (so that it fits neatly in the GPU cache)");

struct bvh {
	// We're optimizing for the GPU, so we're not going to have
	// a root node. Instead we'll have BVH_WIDTH root nodes.
	// This is so that we can immediately dispatch a workgroup
	// that's fairly large to do a large number of tests at once.
	std::vector<BvhNode> nodes;
};

// Build a BVH from a model.
bvh buildBvh(const UModel& model);


#endif