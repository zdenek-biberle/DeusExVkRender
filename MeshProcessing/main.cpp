#include <iostream>
#include <span>
#include <set>
#include "../libs/meshoptimizer/meshoptimizer.h"
#include "tinygltf.h"

const std::string our_extension_name = "NONE_deus_ex_vk_render_mesh";

struct vertex {
	float x, y, z;
	float nx, ny, nz;
	float u, v;

	std::partial_ordering operator<=>(const vertex& other) const = default;
};

template <typename T>
int gltf_component_type_of() {
	if constexpr (std::is_same_v<T, uint8_t>)
		return TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
	else if constexpr (std::is_same_v<T, uint16_t>)
		return TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
	else if constexpr (std::is_same_v<T, uint32_t>)
		return TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
	else if constexpr (std::is_same_v<T, float>)
		return TINYGLTF_COMPONENT_TYPE_FLOAT;
	else
		static_assert(false, "Unsupported type");
}

int num_components_in_gltf_type(int type) {
	switch (type) {
	case TINYGLTF_TYPE_SCALAR:
		return 1;
	case TINYGLTF_TYPE_VEC2:
		return 2;
	case TINYGLTF_TYPE_VEC3:
		return 3;
	case TINYGLTF_TYPE_VEC4:
		return 4;
	case TINYGLTF_TYPE_MAT2:
		return 4;
	case TINYGLTF_TYPE_MAT3:
		return 9;
	case TINYGLTF_TYPE_MAT4:
		return 16;
	default:
		throw std::runtime_error("Unsupported type");
	}
}

template <typename T>
std::pair<std::span<const T>, size_t> strided_accessor_pointer(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
	auto& buffer_view = model.bufferViews.at(accessor.bufferView);
	auto& buffer = model.buffers.at(buffer_view.buffer);
	if (accessor.componentType != gltf_component_type_of<T>())
		throw std::runtime_error("Accessor has wrong component type");
	auto num_components = num_components_in_gltf_type(accessor.type);
	auto byte_stride = buffer_view.byteStride == 0 ? sizeof(T) * num_components : buffer_view.byteStride;
	if (byte_stride % sizeof(T) != 0)
		throw std::runtime_error("Accessor has misaligned stride");
	auto stride = byte_stride / sizeof(T);
	if (stride < num_components)
		throw std::runtime_error("Accessor has stride smaller than the number of components");
	if (accessor.byteOffset + accessor.count * byte_stride > buffer_view.byteLength)
		throw std::runtime_error("Accessor is out of bounds of the buffer view");
	if (buffer_view.byteOffset + accessor.byteOffset + accessor.count * byte_stride > buffer.data.size())
		throw std::runtime_error("Accessor is out of bounds of the buffer");
	auto bytes = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
	if ((reinterpret_cast<uintptr_t>(bytes) % alignof(T)) != 0)
		throw std::runtime_error("Accessor is not aligned");
	if (accessor.count == 0)
		throw std::runtime_error("Accessor has zero count");
	auto extent = accessor.count * stride - stride + num_components;
	return { { reinterpret_cast<const T*>(bytes), extent }, stride };
}

template <typename T>
std::span<const T> packed_accessor_span(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
	auto [span, stride] = strided_accessor_pointer<T>(model, accessor);
	if (stride != 1)
		throw std::runtime_error("Accessor is not packed");
	return span;
}

struct index_accessor_converter {
	static index_accessor_converter create(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
		if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
			return { packed_accessor_span<uint32_t>(model, accessor) };
		}
		else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
			auto span = packed_accessor_span<uint16_t>(model, accessor);
			return { std::vector<uint32_t>(span.begin(), span.end()) };
		}
		else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
			auto span = packed_accessor_span<uint8_t>(model, accessor);
			return { std::vector<uint32_t>{span.begin(), span.end() } };
		}
		else {
			throw std::runtime_error("Unsupported component type for index buffer");
		}
	}

	index_accessor_converter(const std::vector<uint32_t>& buffer) : buffer(buffer) {}
	index_accessor_converter(std::span<const uint32_t> span) : span(span) {}

	std::span<const uint32_t> get_span() const {
		return span.data() ? span : std::span(buffer);
	}

private:
	const std::vector<uint32_t> buffer;
	const std::span<const uint32_t> span;
};

template <typename T>
T span_at(const std::span<const T>& span, size_t index) {
	if (index >= span.size())
		throw std::runtime_error("Index out of bounds");
	return span[index];
}

int main(int argc, char** argv) {
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " input.gltf output.gltf" << std::endl;
		return 1;
	}

	std::string in_filename = argv[1];
	std::string out_filename = argv[2];

	tinygltf::TinyGLTF tinygltf;
	tinygltf::Model model_loaded;
	const tinygltf::Model& model_in = model_loaded;
	std::string load_err;
	std::string load_warn;
	if (!tinygltf.LoadASCIIFromFile(&model_loaded, &load_err, &load_warn, in_filename)) {
		std::cerr << "TinyGLTF failed to load " << in_filename << ": " << load_err << std::endl;
		return 1;
	}

	if (!load_warn.empty()) {
		std::cerr << "TinyGLTF warning: " << load_warn << std::endl;
		return 1;
	}

	if (std::find(model_in.extensionsUsed.cbegin(), model_in.extensionsUsed.cend(), our_extension_name) != model_in.extensionsUsed.cend()) {
		std::cerr << "Model already uses Meshlet extension" << std::endl;
		return 1;
	}

	if (std::find(model_in.extensionsRequired.cbegin(), model_in.extensionsRequired.cend(), our_extension_name) != model_in.extensionsRequired.cend()) {
		std::cerr << "Model already uses Meshlet extension" << std::endl;
		return 1;
	}

	tinygltf::Model model_out{ model_in };
	model_out.buffers.clear();
	model_out.bufferViews.clear();
	model_out.accessors.clear();
	model_out.meshes.clear();

	const int max_vertices = 64;
	const int max_triangles = 124;
	const float cone_weight = 0.0f;
	model_out.extensionsUsed.push_back(our_extension_name);
	model_out.extensions[our_extension_name] = tinygltf::Value(tinygltf::Value::Object{
		{"max_vertices", tinygltf::Value(static_cast<int>(max_vertices))},
		{"max_triangles", tinygltf::Value(static_cast<int>(max_triangles))},
		});


	// Normally, we would have a buffer of vertices and a buffer of indices.
	// However, with meshlets this is slightly different, as we expect to maximize
	// vertex reuse. We'll still have the the same vertices, but we'll have two
	// index buffers: one has the indexes of the vertices used by a given meshlet,
	// the other has indexes of these indexes that form triangles. Since this other 
	// index points into a small range of the first index buffer, we can use a smaller
	// data type for it. meshoptimizer uses uint8_t.
	// Since gltf can't really represent this, we'll still have a regular index buffer,
	// but this will be just an unpacking of the double layers of indexes. It's just for
	// show though, we'll store the other two buffers in an extension.

	// Regular vertex data
	std::vector<vertex> vertex_buffer;
	// Maps a vertex to an index in the vertex buffer. Used to share vertices.
	std::map<vertex, uint32_t> vertex_map;
	// Example: 4 5 6, 5 6 7
	std::vector<uint32_t> unpacked_index_buffer;
	// Example: 4 5 6 7
	std::vector<uint32_t> meshlet_vertex_index_buffer;
	// Example: 0 1 2, 1 2 3
	std::vector<uint8_t> meshlet_local_index_buffer;

	const int vertex_buffer_idx = model_out.buffers.size();
	model_out.buffers.emplace_back();
	const int unpacked_index_buffer_idx = model_out.buffers.size();
	model_out.buffers.emplace_back();
	const int meshlet_vertex_index_buffer_idx = model_out.buffers.size();
	model_out.buffers.emplace_back();
	const int meshlet_local_index_buffer_idx = model_out.buffers.size();
	model_out.buffers.emplace_back();

	const auto vertex_buffer_view_idx = model_out.bufferViews.size();
	model_out.bufferViews.emplace_back();
	const auto unpacked_index_buffer_view_idx = model_out.bufferViews.size();
	model_out.bufferViews.emplace_back();
	const auto meshlet_vertex_index_buffer_view_idx = model_out.bufferViews.size();
	model_out.bufferViews.emplace_back();
	const auto meshlet_local_index_buffer_view_idx = model_out.bufferViews.size();
	model_out.bufferViews.emplace_back();

	const auto positions_accessor_idx = model_out.accessors.size();
	model_out.accessors.emplace_back();
	const auto normals_accessor_idx = model_out.accessors.size();
	model_out.accessors.emplace_back();
	const auto uvs_accessor_idx = model_out.accessors.size();
	model_out.accessors.emplace_back();

	for (auto& mesh : model_in.meshes) {
		std::vector<tinygltf::Primitive> new_primitives;
		for (auto& primitive : mesh.primitives) {
			if (primitive.indices < 0) {
				std::cerr << "Primitive has no indices" << std::endl;
				return 1;
			}

			if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
				std::cerr << "Primitive is not triangles" << std::endl;
				return 1;
			}

			auto& indices_accessor = model_in.accessors.at(primitive.indices);
			auto& positions_accessor = model_in.accessors.at(primitive.attributes.at("POSITION"));
			auto& normal_accessor = model_in.accessors.at(primitive.attributes.at("NORMAL"));
			auto& uv_accessor = model_in.accessors.at(primitive.attributes.at("TEXCOORD_0"));

			if (positions_accessor.type != TINYGLTF_TYPE_VEC3) {
				std::cerr << "POSITION attribute is not a vec3" << std::endl;
				return 1;
			}

			if (normal_accessor.type != TINYGLTF_TYPE_VEC3) {
				std::cerr << "NORMAL attribute is not a vec3" << std::endl;
				return 1;
			}

			if (uv_accessor.type != TINYGLTF_TYPE_VEC2) {
				std::cerr << "TEXCOORD_0 attribute is not a vec2" << std::endl;
				return 1;
			}

			size_t max_meshlets = meshopt_buildMeshletsBound(indices_accessor.count, max_vertices, max_triangles);

			std::vector<meshopt_Meshlet> meshlets(max_meshlets);
			std::vector<unsigned int> meshlet_vertex_indices(max_meshlets * max_vertices);
			std::vector<unsigned char> meshlet_local_indices(max_meshlets * max_triangles * 3);

			//auto indices_ptr = packed_accessor_pointer<uint32_t>(model_in, indices_accessor);
			const auto index_accessor_converter = index_accessor_converter::create(model_in, indices_accessor);
			auto index_span = index_accessor_converter.get_span();
			auto [positions_span, positions_stride] = strided_accessor_pointer<float>(model_in, positions_accessor);
			auto [normals_span, normals_stride] = strided_accessor_pointer<float>(model_in, normal_accessor);
			auto [uvs_span, uvs_stride] = strided_accessor_pointer<float>(model_in, uv_accessor);
			size_t meshlet_count = meshopt_buildMeshlets(
				meshlets.data(),
				meshlet_vertex_indices.data(),
				meshlet_local_indices.data(),
				index_span.data(),
				index_span.size(),
				positions_span.data(),
				positions_accessor.count,
				positions_stride * sizeof(float),
				max_vertices,
				max_triangles,
				cone_weight);

			std::cout << "Processed " << indices_accessor.count << " indices into " << meshlet_count << " meshlets" << std::endl;

			const meshopt_Meshlet& last_meshlet = meshlets[meshlet_count - 1];

			meshlet_vertex_indices.resize(last_meshlet.vertex_offset + last_meshlet.vertex_count);
			meshlet_local_indices.resize(last_meshlet.triangle_offset + ((last_meshlet.triangle_count * 3 + 3) & ~3));
			meshlets.resize(meshlet_count);

			const auto index_of_first_meshlet_vertex_index = meshlet_vertex_index_buffer.size();
			const auto index_of_first_meshlet_local_index = meshlet_local_index_buffer.size();

			for (auto meshlet_local_index : meshlet_local_indices) {
				meshlet_local_index_buffer.push_back(meshlet_local_index);
			}


			for (auto meshlet_vertex_index : meshlet_vertex_indices) {
				// The old index is the index of the vertex in the original vertex buffer.
				// We'll need to remap that to the index of the vertex in the new vertex buffer.
				// To do that, we'll just insert each referenced vertex into the new vertex buffer
				// and use the index of that insertion as the new index.
				
				vertex the_vertex{
					.x = span_at(positions_span, meshlet_vertex_index * positions_stride + 0),
					.y = span_at(positions_span, meshlet_vertex_index * positions_stride + 1),
					.z = span_at(positions_span, meshlet_vertex_index * positions_stride + 2),
					.nx = span_at(normals_span, meshlet_vertex_index * normals_stride + 0),
					.ny = span_at(normals_span, meshlet_vertex_index * normals_stride + 1),
					.nz = span_at(normals_span, meshlet_vertex_index * normals_stride + 2),
					.u = span_at(uvs_span, meshlet_vertex_index * uvs_stride + 0),
					.v = span_at(uvs_span, meshlet_vertex_index * uvs_stride + 1),
				};
				if (auto existing_idx_it = vertex_map.find(the_vertex); existing_idx_it != vertex_map.end()) {
					// reusing the vertex
					meshlet_vertex_index_buffer.push_back(existing_idx_it->second);
				}
				else {
					// adding a new vertex
					meshlet_vertex_index_buffer.push_back(vertex_buffer.size());
					vertex_map[the_vertex] = vertex_buffer.size();
					vertex_buffer.push_back(the_vertex);
				}
			}

			// Now we go through all the meshlets and:
			// - unpack the indices into the unpacked_index_buffer
			// - copy meshlet_vertex_indices into meshlet_vertex_index_buffer
			// - copy meshlet_local_indices into meshlet_local_index_buffer
			// - create new accessors for all three
			// - link the meshlet accessors to the regular index accessor via the extension
			// - create a new primitive
			// TODO: meshopt_optimizeMeshlet? other optimizations?

			for (auto& m : meshlets) {
				const auto index_of_first_unpacked_index_for_meshlet = unpacked_index_buffer.size();
				for (int index_of_local_index = 0; index_of_local_index < m.triangle_count * 3; ++index_of_local_index) {
					const auto meshlet_local_index = meshlet_local_indices.at(m.triangle_offset + index_of_local_index);
					// meshlet_local_index points into meshlet_vertex_indices at m.vertex_offset + meshlet_local_index,
					// which then points to the vertex in the old vertex buffer. However, we've remapped these to the new
					// vertex buffer - so instead of looking up indices in meshlet_vertex_indices, we'll look them up in
					// meshlet_vertex_index_buffer at index_of_first_meshlet_vertex_index + m.vertex_offset + meshlet_local_index.
					const auto meshlet_vertex_index = meshlet_vertex_index_buffer.at(index_of_first_meshlet_vertex_index + m.vertex_offset + meshlet_local_index);
					unpacked_index_buffer.push_back(meshlet_vertex_index);
				}

				const int meshlet_vertex_index_accessor_index = model_out.accessors.size();
				tinygltf::Accessor meshlet_vertex_index_accessor;
				meshlet_vertex_index_accessor.bufferView = meshlet_vertex_index_buffer_view_idx;
				meshlet_vertex_index_accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
				meshlet_vertex_index_accessor.type = TINYGLTF_TYPE_SCALAR;
				meshlet_vertex_index_accessor.count = m.vertex_count;
				meshlet_vertex_index_accessor.byteOffset = (index_of_first_meshlet_vertex_index + m.vertex_offset) * sizeof(uint32_t);
				model_out.accessors.push_back(meshlet_vertex_index_accessor);

				const int meshlet_local_index_accessor_index = model_out.accessors.size();
				tinygltf::Accessor meshlet_local_index_accessor;
				meshlet_local_index_accessor.bufferView = meshlet_local_index_buffer_view_idx;
				meshlet_local_index_accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
				meshlet_local_index_accessor.type = TINYGLTF_TYPE_SCALAR;
				meshlet_local_index_accessor.count = m.triangle_count * 3;
				meshlet_local_index_accessor.byteOffset = (index_of_first_meshlet_local_index + m.triangle_offset) * sizeof(uint8_t);
				model_out.accessors.push_back(meshlet_local_index_accessor);

				const auto unpacked_index_accessor_index = model_out.accessors.size();
				tinygltf::Accessor unpacked_index_accessor;
				unpacked_index_accessor.bufferView = unpacked_index_buffer_view_idx;
				unpacked_index_accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
				unpacked_index_accessor.type = TINYGLTF_TYPE_SCALAR;
				unpacked_index_accessor.count = m.triangle_count * 3;
				unpacked_index_accessor.byteOffset = index_of_first_unpacked_index_for_meshlet * sizeof(uint32_t);
				model_out.accessors.push_back(unpacked_index_accessor);

				tinygltf::Primitive new_primitive;
				new_primitive.attributes = {
					{ "POSITION", positions_accessor_idx },
					{ "NORMAL", normals_accessor_idx },
					{ "TEXCOORD_0", uvs_accessor_idx },
				};
				new_primitive.indices = unpacked_index_accessor_index;
				new_primitive.material = primitive.material;
				new_primitive.mode = TINYGLTF_MODE_TRIANGLES;
				new_primitive.extensions.insert({ our_extension_name, tinygltf::Value(tinygltf::Value::Object{
					{"meshlet_vertex_indices", tinygltf::Value(meshlet_vertex_index_accessor_index)},
					{"meshlet_local_indices", tinygltf::Value(meshlet_local_index_accessor_index)},
					}) });
				new_primitives.push_back(new_primitive);
			}
		}

		tinygltf::Mesh new_mesh;
		new_mesh.name = mesh.name;
		new_mesh.primitives = std::move(new_primitives);
		model_out.meshes.push_back(new_mesh);
	}

	std::cout << "Got " << unpacked_index_buffer.size() << " unpacked indices, " << meshlet_vertex_index_buffer.size() << " meshlet vertex indices, " << meshlet_local_index_buffer.size() << " meshlet local indices, and " << vertex_buffer.size() << " vertices" << std::endl;

	float min_x = std::numeric_limits<float>::infinity(), max_x = -std::numeric_limits<float>::infinity();
	float min_y = std::numeric_limits<float>::infinity(), max_y = -std::numeric_limits<float>::infinity();
	float min_z = std::numeric_limits<float>::infinity(), max_z = -std::numeric_limits<float>::infinity();

	for (auto& vertex : vertex_buffer) {
		min_x = std::min(min_x, vertex.x);
		max_x = std::max(max_x, vertex.x);
		min_y = std::min(min_y, vertex.y);
		max_y = std::max(max_y, vertex.y);
		min_z = std::min(min_z, vertex.z);
		max_z = std::max(max_z, vertex.z);
	}

	tinygltf::Accessor positions_accessor;
	positions_accessor.bufferView = vertex_buffer_view_idx;
	positions_accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	positions_accessor.type = TINYGLTF_TYPE_VEC3;
	positions_accessor.count = vertex_buffer.size();
	positions_accessor.byteOffset = offsetof(vertex, x);
	positions_accessor.minValues = { min_x, min_y, min_z };
	positions_accessor.maxValues = { max_x, max_y, max_z };
	model_out.accessors.at(positions_accessor_idx) = positions_accessor;

	tinygltf::Accessor normals_accessor;
	normals_accessor.bufferView = vertex_buffer_view_idx;
	normals_accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	normals_accessor.type = TINYGLTF_TYPE_VEC3;
	normals_accessor.count = vertex_buffer.size();
	normals_accessor.byteOffset = offsetof(vertex, nx);
	model_out.accessors.at(normals_accessor_idx) = normals_accessor;

	tinygltf::Accessor uvs_accessor;
	uvs_accessor.bufferView = vertex_buffer_view_idx;
	uvs_accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	uvs_accessor.type = TINYGLTF_TYPE_VEC2;
	uvs_accessor.count = vertex_buffer.size();
	uvs_accessor.byteOffset = offsetof(vertex, u);
	model_out.accessors.at(uvs_accessor_idx) = uvs_accessor;

	tinygltf::BufferView vertex_buffer_view;
	vertex_buffer_view.name = "vertex buffer";
	vertex_buffer_view.buffer = vertex_buffer_idx;
	vertex_buffer_view.byteOffset = 0;
	vertex_buffer_view.byteLength = vertex_buffer.size() * sizeof(vertex);
	vertex_buffer_view.byteStride = sizeof(vertex);
	model_out.bufferViews.at(vertex_buffer_view_idx) = vertex_buffer_view;

	tinygltf::BufferView unpacked_index_buffer_view;
	unpacked_index_buffer_view.name = "index buffer";
	unpacked_index_buffer_view.buffer = unpacked_index_buffer_idx;
	unpacked_index_buffer_view.byteOffset = 0;
	unpacked_index_buffer_view.byteLength = unpacked_index_buffer.size() * sizeof(uint32_t);
	model_out.bufferViews.at(unpacked_index_buffer_view_idx) = unpacked_index_buffer_view;

	tinygltf::BufferView meshlet_vertex_index_buffer_view;
	meshlet_vertex_index_buffer_view.name = "meshlet vertex index buffer";
	meshlet_vertex_index_buffer_view.buffer = meshlet_vertex_index_buffer_idx;
	meshlet_vertex_index_buffer_view.byteOffset = 0;
	meshlet_vertex_index_buffer_view.byteLength = meshlet_vertex_index_buffer.size() * sizeof(uint32_t);
	model_out.bufferViews.at(meshlet_vertex_index_buffer_view_idx) = meshlet_vertex_index_buffer_view;

	tinygltf::BufferView meshlet_local_index_buffer_view;
	meshlet_local_index_buffer_view.name = "meshlet local index buffer";
	meshlet_local_index_buffer_view.buffer = meshlet_local_index_buffer_idx;
	meshlet_local_index_buffer_view.byteOffset = 0;
	meshlet_local_index_buffer_view.byteLength = meshlet_local_index_buffer.size() * sizeof(uint8_t);
	model_out.bufferViews.at(meshlet_local_index_buffer_view_idx) = meshlet_local_index_buffer_view;

	tinygltf::Buffer gltf_vertex_buffer;
	gltf_vertex_buffer.name = "vertex buffer";
	gltf_vertex_buffer.data = { reinterpret_cast<unsigned char*>(vertex_buffer.data()), reinterpret_cast<unsigned char*>(vertex_buffer.data() + vertex_buffer.size()) };
	model_out.buffers.at(vertex_buffer_idx) = gltf_vertex_buffer;

	tinygltf::Buffer gltf_unpacked_index_buffer;
	gltf_unpacked_index_buffer.name = "index buffer";
	gltf_unpacked_index_buffer.data = { reinterpret_cast<unsigned char*>(unpacked_index_buffer.data()), reinterpret_cast<unsigned char*>(unpacked_index_buffer.data() + unpacked_index_buffer.size()) };
	model_out.buffers.at(unpacked_index_buffer_idx) = gltf_unpacked_index_buffer;

	tinygltf::Buffer gltf_meshlet_vertex_index_buffer;
	gltf_meshlet_vertex_index_buffer.name = "meshlet vertex index buffer";
	gltf_meshlet_vertex_index_buffer.data = { reinterpret_cast<unsigned char*>(meshlet_vertex_index_buffer.data()), reinterpret_cast<unsigned char*>(meshlet_vertex_index_buffer.data() + meshlet_vertex_index_buffer.size()) };
	model_out.buffers.at(meshlet_vertex_index_buffer_idx) = gltf_meshlet_vertex_index_buffer;

	tinygltf::Buffer gltf_meshlet_local_index_buffer;
	gltf_meshlet_local_index_buffer.name = "meshlet local index buffer";
	gltf_meshlet_local_index_buffer.data = { reinterpret_cast<unsigned char*>(meshlet_local_index_buffer.data()), reinterpret_cast<unsigned char*>(meshlet_local_index_buffer.data() + meshlet_local_index_buffer.size()) };
	model_out.buffers.at(meshlet_local_index_buffer_idx) = gltf_meshlet_local_index_buffer;

	if (!tinygltf.WriteGltfSceneToFile(&model_out, out_filename, false, false, true, false)) {
		std::cerr << "TinyGLTF failed to write " << out_filename << std::endl;
		return 1;
	}

	return 0;
}