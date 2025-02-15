#include "Precomp.h"
#include "bvh.h"

// This is probably not gonna be very smart or efficient right now,
// but we gotta start somewhere.
//
// Instead of building the BVH using any existing algorithms, we're
// basically just gonna turn the model's BSP into a BVH. Essentially,
// we'll turn each log2(BVH_WIDTH) layers of the BSP tree into a single
// node.
//
// Note that this algorithm is kinda stupid and probably won't work -
// it sort of ignores the fact that all nodes in the BSP tree can actually
// contain geometry.
struct builder {
	const UModel& model;

	// The BVH we're building.
	std::vector<BvhNode> nodes;

	// Maps BVH nodes indices to corresponding BSP node indices.
	// Could be INDEX_NONE if no corresponding BSP node exists.
	std::vector<int> bvh2bsp;

	BvhNode go(int bspNodeIdx);

	void goLevel(int bspNodeIdx, uint level);

	void print(uint idx, uint depth);
};

static const BvhNode empty_bvh_node{
	FVector{ 0, 0, 0 },
	FVector{ 0, 0, 0 },
	BVH_INDEX_NONE,
	1
};

// asserts are weird, so we use our own
#define CHECK(x) if (!(x)) { throw std::runtime_error("Assertion failed: " #x); }

bvh buildBvh(const UModel& model)
{
	builder b{ model };
	auto top_node = b.go(0); // TODO: is index 0 the root node?

	debugf(L"BVH built, got a top level node: [%f %f %f]:[%f %f %f], %d, %d",
		top_node.min.X, top_node.min.Y, top_node.min.Z,
		top_node.max.X, top_node.max.Y, top_node.max.Z,
		top_node.firstChildIdx, top_node.leaf);

	debugf(L"Total nodes: %d", b.nodes.size());

	for (uint i = 0; i < std::min(b.nodes.size(), BVH_WIDTH); i++) {
		b.print(i, 0);
	}

	return bvh{
		std::move(b.nodes)
	};
}

BvhNode builder::go(int bspNodeIdx)
{
	// first we check if we're an empty fake leaf node
	if (bspNodeIdx == INDEX_NONE) {
		return empty_bvh_node;
	}

	// we're a legit node, yay
	auto& node = model.Nodes(bspNodeIdx);

	// next we just collect indices of the appropriate BSP nodes
	CHECK(nodes.size() == bvh2bsp.size());
	CHECK(nodes.size() % BVH_WIDTH == 0);
	CHECK(bvh2bsp.size() % BVH_WIDTH == 0);
	uint firstNodeIdx = nodes.size();
	goLevel(bspNodeIdx, 0);

	// How many did we get?
	uint numNodes = bvh2bsp.size() - firstNodeIdx;
	CHECK(numNodes <= BVH_WIDTH);

	// If we got zero, then we're a leaf node.
	if (numNodes == 0) {
		if (node.NumVertices >= 3) {
			// gotta calculate the aabb ourselves
			FBox aabb(0);
			for (int i = 0; i < node.NumVertices; i++) {
				aabb += model.Points(model.Verts(node.iVertPool + i).pVertex);
			}

			return {
				aabb.Min,
				aabb.Max,
				BVH_INDEX_NONE,
				1
			};
		}
		else {
			// we're a leaf node with no geometry
			return empty_bvh_node;
		}
	}

	// If we got more than zero children, but more than
	// zero, then we're just gonna pad the node with
	// some dummy children.
	for (uint i = numNodes; i < BVH_WIDTH; ++i) {
		bvh2bsp.push_back(INDEX_NONE);
	}

	CHECK(bvh2bsp.size() % BVH_WIDTH == 0);
	CHECK(bvh2bsp.size() == nodes.size() + BVH_WIDTH);

	// now recurse into all the children and fill the nodes array
	nodes.resize(bvh2bsp.size());
	for (uint i = firstNodeIdx; i < firstNodeIdx + BVH_WIDTH; ++i) {
		nodes[i] = go(bvh2bsp[i]);
	}

	// now calculate the AABB
	FBox aabb(0);
	for (uint i = firstNodeIdx; i < firstNodeIdx + BVH_WIDTH; ++i) {
		auto& node = nodes[i];
		if (!node.isEmpty()) {
			aabb += nodes[i].min;
			aabb += nodes[i].max;
		}
	}

	return {
		aabb.Min,
		aabb.Max,
		firstNodeIdx,
		0
	};
}

void builder::goLevel(int bspNodeIdx, uint level)
{
	CHECK(bspNodeIdx != INDEX_NONE);

	if (level == BVH_WIDTH_LOG2) {
		// We're done, add ourselves to the BVH.
		bvh2bsp.push_back(bspNodeIdx);
	}
	else {
		auto& node = model.Nodes(bspNodeIdx);
		if (node.iBack != INDEX_NONE && node.iFront != INDEX_NONE) {
			// Recurse into children.
			goLevel(node.iBack, level + 1);
			goLevel(node.iFront, level + 1);
		}
		else if (level > 0 && node.iBack == INDEX_NONE && node.iFront == INDEX_NONE) {
			// We're a leaf node, add ourselves.
			bvh2bsp.push_back(bspNodeIdx);
		}
		else if (level > 0) {
			// We're a weird node with only one child, so we're gonna add
			// ourselves. Very inefficient!
			bvh2bsp.push_back(bspNodeIdx);
		}
	}
}

void builder::print(uint idx, uint depth) {
	auto& node = nodes[idx];
	if (node.isEmpty()) {
		debugf(L"%*s- Empty", depth, L"");
		return;
	}
	CHECK(node.min.X <= node.max.X);
	CHECK(node.min.Y <= node.max.Y);
	CHECK(node.min.Z <= node.max.Z);

	debugf(L"%*s- [%f %f %f]:[%f %f %f], V: %f, A: %f, first child: %d, leaf: %d",
		depth, L"",
		node.min.X, node.min.Y, node.min.Z,
		node.max.X, node.max.Y, node.max.Z,
		node.volume(), node.area(),
		node.firstChildIdx, node.leaf);

	if (!node.leaf) {
		for (uint i = 0; i < BVH_WIDTH; i++) {
			print(node.firstChildIdx + i, depth + 2);
		}
	}
}

bool BvhNode::isEmpty() const
{
	return min == max;
}

float BvhNode::area() const
{
	return 2 * (
		(max.X - min.X) * (max.Y - min.Y)
		+ (max.X - min.X) * (max.Z - min.Z)
		+ (max.Y - min.Y) * (max.Z - min.Z)
		);
}

float BvhNode::volume() const
{
	return (max.X - min.X) * (max.Y - min.Y) * (max.Z - min.Z);
}