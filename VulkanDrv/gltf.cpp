#include "Precomp.h"
#include "gltf.h"
#include <codecvt>

struct output_vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texcoord;
};

typedef UINT u32;

static glm::vec3 encode_point(const FVector& point) {
	// Get coords into the coordinate space of glTF. Also, scale it down to
	// meters, approximately (we're using the 0.75 inches per UU scale as per
	// https://beyondunrealwiki.github.io/pages/general-scale-and-dimension.html).
	//return FVector(point.X, point.Z, point.Y) * 0.01905f;
	// actually, screw that, use a power-of-2 scale so that we get nicer numbers
	return glm::vec3{ point.X, point.Z, point.Y } / 64.f;
}

static glm::vec3 encode_vector(const FVector& vector) {
	return { vector.X, vector.Z, vector.Y };
}

static glm::vec3 decode_point(const glm::vec3& point) {
	return glm::vec3{ point.x, point.z, point.y } * 64.f;
}

static glm::vec3 decode_vector(const glm::vec3& vector) {
	return { vector.x, vector.z, vector.y };
}

void save_model_to_gltf(const UModel* model) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> string_converter;
	auto filename = "gltf/" + string_converter.to_bytes(model->GetFullName()) + ".gltf";
	if (tinygltf::FileExists(filename, nullptr)) {
		debugf(L"Model %s@%p already exists as %S, skipping glTF export", model->GetFullName(), model, filename.c_str());
		return;
	}

	// Group all the nodes per texture.
	// TODO: Should we also perhaps consider polyflags?
	std::map<UTexture*, std::vector<int>> node_indices_per_texture;

	for (int node_idx = 0; node_idx < model->Nodes.Num(); node_idx++) {
		auto& node = model->Nodes(node_idx);
		if (node.NumVertices < 3) continue;
		auto& surf = model->Surfs(node.iSurf);
		if (surf.PolyFlags & PF_Invisible) continue;
		if (!surf.Texture) {
			debugf(L"Model %s@%p has null texture for surf %d", model->GetFullName(), model, node.iSurf);
			continue;
		}
		node_indices_per_texture[surf.Texture].push_back(node_idx);
	}

	tinygltf::Model gltf;

	tinygltf::Sampler default_sampler;
	default_sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
	default_sampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
	const auto default_sampler_idx = gltf.samplers.size();
	gltf.samplers.push_back(std::move(default_sampler));

	// Create buffers.
	// We gotta fill in the data later.
	const auto vertex_buffer_idx = gltf.buffers.size();
	gltf.buffers.emplace_back();
	const auto index_buffer_idx = gltf.buffers.size();
	gltf.buffers.emplace_back();

	// Create buffer views.
	// We gotta fill in the byte length later.
	tinygltf::BufferView vertex_buffer_view;
	vertex_buffer_view.buffer = vertex_buffer_idx;
	vertex_buffer_view.byteStride = sizeof(output_vertex);
	vertex_buffer_view.target = TINYGLTF_TARGET_ARRAY_BUFFER;
	const auto vertex_data_buffer_view_index = gltf.bufferViews.size();
	gltf.bufferViews.push_back(std::move(vertex_buffer_view));
	tinygltf::BufferView index_buffer_view;
	index_buffer_view.buffer = index_buffer_idx;
	index_buffer_view.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
	const auto index_data_buffer_view_index = gltf.bufferViews.size();
	gltf.bufferViews.push_back(std::move(index_buffer_view));

	// Create accessors. Vertex data is all going to use the same buffer view.
	// We gotta fill in the counts later.
	tinygltf::Accessor position_accessor;
	position_accessor.bufferView = vertex_data_buffer_view_index;
	position_accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	position_accessor.type = TINYGLTF_TYPE_VEC3;
	position_accessor.byteOffset = offsetof(output_vertex, position);
	const auto position_accessor_idx = gltf.accessors.size();
	gltf.accessors.push_back(std::move(position_accessor));
	tinygltf::Accessor normal_accessor;
	normal_accessor.bufferView = vertex_data_buffer_view_index;
	normal_accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	normal_accessor.type = TINYGLTF_TYPE_VEC3;
	normal_accessor.byteOffset = offsetof(output_vertex, normal);
	const auto normal_accessor_idx = gltf.accessors.size();
	gltf.accessors.push_back(std::move(normal_accessor));
	tinygltf::Accessor texcoord_accessor;
	texcoord_accessor.bufferView = vertex_data_buffer_view_index;
	texcoord_accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
	texcoord_accessor.type = TINYGLTF_TYPE_VEC2;
	texcoord_accessor.byteOffset = offsetof(output_vertex, texcoord);
	const auto texcoord_accessor_idx = gltf.accessors.size();
	gltf.accessors.push_back(std::move(texcoord_accessor));

	// These will be our buffers. We'll then copy them into the gltf model.
	std::vector<output_vertex> vertices;
	std::vector<u32> indices;

	tinygltf::Mesh mesh;
	FBox bounding_box(0);
	mesh.name = string_converter.to_bytes(model->GetFullName());
	for (auto& [texture, node_indices] : node_indices_per_texture) {
		// create a new image
		tinygltf::Image image;
		image.name = string_converter.to_bytes(texture->GetFullName());
		image.mimeType = "image/png";
		//image.uri = "ue:" + image.name;
		image.width = texture->USize;
		image.height = texture->VSize;
		image.component = 4;
		image.bits = 8;
		image.pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
		auto& mip = texture->Mips(0);
		auto mipData = static_cast<BYTE*>(mip.DataArray.GetData());
		auto& palette = texture->Palette->Colors;
		for (int v = 0; v < mip.VSize; v++) {
			for (int u = 0; u < mip.USize; u++) {
				auto color = palette(mipData[v * mip.USize + u]);
				image.image.push_back(color.R);
				image.image.push_back(color.G);
				image.image.push_back(color.B);
				image.image.push_back(color.A);
			}
		}

		const auto image_idx = gltf.images.size();
		gltf.images.push_back(std::move(image));

		// create a new texture
		tinygltf::Texture gltf_texture;
		gltf_texture.source = image_idx;
		gltf_texture.sampler = default_sampler_idx;
		const auto texture_idx = gltf.textures.size();
		gltf.textures.push_back(std::move(gltf_texture));

		// create a new material
		tinygltf::Material material;
		material.pbrMetallicRoughness.baseColorTexture.index = texture_idx;
		/*material.pbrMetallicRoughness.baseColorFactor = {
			texture->MipZero.R / 255.,
			texture->MipZero.G / 255.,
			texture->MipZero.B / 255.,
			texture->MipZero.A / 255.,
		};*/
		material.pbrMetallicRoughness.baseColorTexture.texCoord = 0;
		const auto material_idx = gltf.materials.size();
		gltf.materials.push_back(std::move(material));

		const auto first_vertex_idx = vertices.size();
		const auto first_index_idx = indices.size();

		for (auto node_idx : node_indices) {
			auto& node = model->Nodes(node_idx);
			auto& surf = model->Surfs(node.iSurf);
			const auto base = model->Points(surf.pBase);
			const auto tex_u = model->Vectors(surf.vTextureU) / texture->USize;
			const auto tex_v = model->Vectors(surf.vTextureV) / texture->VSize;
			const auto tex_base_u = (base | tex_u) - surf.PanU / static_cast<float>(texture->USize);
			const auto tex_base_v = (base | tex_v) - surf.PanV / static_cast<float>(texture->VSize);
			const auto normal = model->Vectors(surf.vNormal);
			const auto encoded_normal = encode_vector(normal);
			const auto first_vertex_idx_for_node = vertices.size();

			for (int j = 0; j < node.NumVertices; j++) {
				const auto point = model->Points(model->Verts(node.iVertPool + j).pVertex);

				const auto encoded_point = encode_point(point);
				bounding_box += { encoded_point.x, encoded_point.b, encoded_point.z };
				vertices.push_back(output_vertex{
					encoded_point,
					encoded_normal,
					{
						(point | tex_u) - tex_base_u,
						(point | tex_v) - tex_base_v,
					},
					});
			}

			// push the triangle indices
			for (int j = 2; j < node.NumVertices; j++) {
				indices.push_back(first_vertex_idx_for_node);
				indices.push_back(first_vertex_idx_for_node + j);
				indices.push_back(first_vertex_idx_for_node + j - 1);
			}
		}

		const auto num_vertices = vertices.size() - first_vertex_idx;
		const auto num_indices = indices.size() - first_index_idx;

		// we can now create accessors for this primitive
		tinygltf::Accessor indices_accessor;
		indices_accessor.bufferView = 1;
		indices_accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
		indices_accessor.type = TINYGLTF_TYPE_SCALAR;
		indices_accessor.count = num_indices;
		indices_accessor.byteOffset = first_index_idx * sizeof(u32);
		const auto indices_accessor_idx = gltf.accessors.size();
		gltf.accessors.push_back(std::move(indices_accessor));

		tinygltf::Primitive primitive;
		primitive.attributes["POSITION"] = position_accessor_idx;
		primitive.attributes["NORMAL"] = normal_accessor_idx;
		primitive.attributes["TEXCOORD_0"] = texcoord_accessor_idx;
		primitive.indices = indices_accessor_idx;
		primitive.material = material_idx;
		primitive.mode = TINYGLTF_MODE_TRIANGLES;
		const auto primitive_idx = mesh.primitives.size();
		mesh.primitives.push_back(std::move(primitive));
	}

	gltf.meshes.push_back(std::move(mesh));

	// Now fill in the buffers and other associated things that we're missing.
	gltf.buffers.at(vertex_buffer_idx).data = std::vector<unsigned char>(reinterpret_cast<unsigned char*>(vertices.data()), reinterpret_cast<unsigned char*>(vertices.data() + vertices.size()));
	gltf.bufferViews.at(vertex_data_buffer_view_index).byteLength = gltf.buffers[vertex_buffer_idx].data.size();
	gltf.accessors.at(position_accessor_idx).count = vertices.size();
	gltf.accessors.at(position_accessor_idx).minValues = { bounding_box.Min.X, bounding_box.Min.Y, bounding_box.Min.Z };
	gltf.accessors.at(position_accessor_idx).maxValues = { bounding_box.Max.X, bounding_box.Max.Y, bounding_box.Max.Z };
	gltf.accessors.at(normal_accessor_idx).count = vertices.size();
	gltf.accessors.at(texcoord_accessor_idx).count = vertices.size();

	gltf.buffers.at(index_buffer_idx).data = std::vector<unsigned char>(reinterpret_cast<unsigned char*>(indices.data()), reinterpret_cast<unsigned char*>(indices.data() + indices.size()));
	gltf.bufferViews.at(index_data_buffer_view_index).byteLength = gltf.buffers[index_buffer_idx].data.size();

	gltf.asset.generator = "DeusExVkRender";
	gltf.asset.copyright = "Ion Storm";

	tinygltf::Node node;
	node.mesh = 0;
	gltf.nodes.push_back(std::move(node));

	tinygltf::Scene scene;
	scene.name = string_converter.to_bytes(model->GetFullName());
	scene.nodes.push_back(0);
	gltf.scenes.push_back(std::move(scene));

	gltf.defaultScene = 0;

	tinygltf::TinyGLTF gltf_writer;
	if (!gltf_writer.WriteGltfSceneToFile(&gltf, filename, false, false, true, false)) {
		throw std::runtime_error("Failed to write gltf file");
	}
}

static const std::string our_extension_name = "NONE_deus_ex_vk_render_mesh";

std::optional<ModelReplacement> load_replacement_for_model(const UModel* model) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> string_converter;
	auto filename = "gltf/" + string_converter.to_bytes(model->GetFullName()) + ".replacement.gltf";
	tinygltf::TinyGLTF gltf_reader;
	tinygltf::Model gltf;
	std::string err;
	std::string warn;

	if (!tinygltf::FileExists(filename, nullptr)) {
		debugf(L"Model %s@%p does not have a replacement", model->GetFullName(), model);
		return std::nullopt;
	}

	if (!gltf_reader.LoadASCIIFromFile(&gltf, &err, &warn, filename)) {
		debugf(L"Failed to load gltf file %S: %S", filename.c_str(), err.c_str());
		throw std::runtime_error(err);
	}

	if (!warn.empty()) {
		debugf(L"Warning loading gltf file %S: %S", filename.c_str(), warn.c_str());
		throw std::runtime_error(warn);
	}

	if (std::find(gltf.extensionsUsed.begin(), gltf.extensionsUsed.end(), our_extension_name) == gltf.extensionsUsed.end()) {
		debugf(L"Model %s@%p does not use our extension", model->GetFullName(), model);
		throw std::runtime_error("Model does not use our extension");
	}

	if (gltf.scenes.size() != 1) {
		debugf(L"%S: Expected exactly one scene, got %d", filename.c_str(), gltf.scenes.size());
		throw std::runtime_error("Expected exactly one scene");
	}

	if (gltf.scenes.at(0).nodes.size() < 1) {
		debugf(L"%S: Expected at least one node in the scene", filename.c_str());
		throw std::runtime_error("Expected exactly one node in the scene");
	}

	const auto& some_primitive = gltf.meshes.at(gltf.nodes.at(gltf.scenes.at(0).nodes.at(0)).mesh).primitives.at(0);

	auto position_accessor_idx = some_primitive.attributes.at("POSITION");
	auto normal_accessor_idx = some_primitive.attributes.at("NORMAL");
	auto texcoord_accessor_idx = some_primitive.attributes.at("TEXCOORD_0");

	auto& position_accessor = gltf.accessors.at(position_accessor_idx);
	auto& normal_accessor = gltf.accessors.at(normal_accessor_idx);
	auto& texcoord_accessor = gltf.accessors.at(texcoord_accessor_idx);

	if (position_accessor.bufferView != normal_accessor.bufferView || position_accessor.bufferView != texcoord_accessor.bufferView) {
		debugf(L"%S: Vertex attribute accessors are not in the same buffer view", filename.c_str());
		throw std::runtime_error("Vertex attribute accessors are not in the same buffer view");
	}

	if (position_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || normal_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || texcoord_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
		debugf(L"%S: Vertex attribute accessors  have wrong component type", filename.c_str());
		throw std::runtime_error("Vertex attribute accessors have wrong component type");
	}

	if (position_accessor.type != TINYGLTF_TYPE_VEC3 || normal_accessor.type != TINYGLTF_TYPE_VEC3 || texcoord_accessor.type != TINYGLTF_TYPE_VEC2) {
		debugf(L"%S: Vertex attribute accessors have wrong type", filename.c_str());
		throw std::runtime_error("Vertex attribute accessors have wrong type");
	}

	if (position_accessor.count != normal_accessor.count || position_accessor.count != texcoord_accessor.count) {
		debugf(L"%S: Vertex attribute accessors have different counts (%d, %d, %d)", filename.c_str(), position_accessor.count, normal_accessor.count, texcoord_accessor.count);
		throw std::runtime_error("Vertex attribute accessors have different counts");
	}

	auto vertex_attribute_buffer_view_idx = position_accessor.bufferView;
	auto& vertex_attribute_buffer_view = gltf.bufferViews.at(vertex_attribute_buffer_view_idx);

	if (vertex_attribute_buffer_view.byteStride != sizeof(MeshletVertex)) {
		debugf(L"%S: Vertex attribute buffer view has wrong stride (%d, expected %d)", filename.c_str(), vertex_attribute_buffer_view.byteStride, sizeof(MeshletVertex));
		throw std::runtime_error("Vertex attribute buffer view has wrong stride");
	}

	if (vertex_attribute_buffer_view.byteLength != sizeof(MeshletVertex) * position_accessor.count) {
		debugf(L"%S: Vertex attribute buffer view has wrong length (%d, expected %d)", filename.c_str(), vertex_attribute_buffer_view.byteLength, sizeof(MeshletVertex) * position_accessor.count);
		throw std::runtime_error("Vertex attribute buffer view has wrong length");
	}

	if (position_accessor.byteOffset != offsetof(MeshletVertex, pos) || normal_accessor.byteOffset != offsetof(MeshletVertex, normal) || texcoord_accessor.byteOffset != offsetof(MeshletVertex, uv)) {
		debugf(L"%S: Vertex attribute accessors have wrong offsets (%d, %d, %d, expected %d, %d, %d)", filename.c_str(), position_accessor.byteOffset, normal_accessor.byteOffset, texcoord_accessor.byteOffset, offsetof(MeshletVertex, pos), offsetof(MeshletVertex, normal), offsetof(MeshletVertex, uv));
		throw std::runtime_error("Vertex attribute accessors have wrong offsets");
	}

	auto& vertex_attribute_buffer = gltf.buffers.at(vertex_attribute_buffer_view.buffer);
	if (vertex_attribute_buffer.data.size() < vertex_attribute_buffer_view.byteOffset + vertex_attribute_buffer_view.byteLength) {
		debugf(L"%S: Vertex attribute buffer is too small (%d, expected at least %d)", filename.c_str(), vertex_attribute_buffer.data.size(), vertex_attribute_buffer_view.byteOffset + vertex_attribute_buffer_view.byteLength);
		throw std::runtime_error("Vertex attribute buffer is too small");
	}

	const auto some_vertex_indices_accessor_idx = some_primitive.extensions.at(our_extension_name).Get("meshlet_vertex_indices").Get<int>();
	const auto some_local_indices_accessor_idx = some_primitive.extensions.at(our_extension_name).Get("meshlet_local_indices").Get<int>();
	const auto& some_vertex_indices_accessor = gltf.accessors.at(some_vertex_indices_accessor_idx);
	const auto& some_local_indices_accessor = gltf.accessors.at(some_local_indices_accessor_idx);
	const auto vertex_indices_buffer_view_idx = some_vertex_indices_accessor.bufferView;
	const auto local_indices_buffer_view_idx = some_local_indices_accessor.bufferView;
	const auto& vertex_indices_buffer_view = gltf.bufferViews.at(vertex_indices_buffer_view_idx);
	const auto& local_indices_buffer_view = gltf.bufferViews.at(local_indices_buffer_view_idx);
	const auto& vertex_indices_buffer = gltf.buffers.at(vertex_indices_buffer_view.buffer);
	const auto& local_indices_buffer = gltf.buffers.at(local_indices_buffer_view.buffer);

	if (vertex_indices_buffer_view.byteStride != 0 || local_indices_buffer_view.byteStride != 0) {
		debugf(L"%S: Meshlet index accessors are not packed (%d, %d)", filename, vertex_indices_buffer_view.byteStride, local_indices_buffer_view.byteStride);
		throw std::runtime_error("Meshlet index accessors are not packed");
	}

	std::vector<Meshlet> meshlets;
	for (auto node_idx : gltf.scenes.at(0).nodes) {
		auto& node = gltf.nodes.at(node_idx);

		if (node.mesh < 0 || node.mesh >= gltf.meshes.size()) {
			debugf(L"%S: Node has invalid mesh index %d", filename.c_str(), node.mesh);
			throw std::runtime_error("Node has invalid mesh index");
		}

		if (node.skin >= 0 || node.light >= 0 || node.camera >= 0 || node.emitter >= 0 || node.lods.size() || node.children.size() || node.rotation.size() || node.scale.size() || node.translation.size() || node.matrix.size() || node.weights.size()) {
			debugf(L"%S: Node uses unsupported features", filename.c_str());
			throw std::runtime_error("Node uses unsupported features");
		}

		auto& mesh = gltf.meshes.at(node.mesh);

		for (auto& primitive : mesh.primitives) {
			const auto vertex_indices_accessor_idx = primitive.extensions.at(our_extension_name).Get("meshlet_vertex_indices").Get<int>();
			const auto local_indices_accessor_idx = primitive.extensions.at(our_extension_name).Get("meshlet_local_indices").Get<int>();
			const auto& vertex_indices_accessor = gltf.accessors.at(vertex_indices_accessor_idx);
			const auto& local_indices_accessor = gltf.accessors.at(local_indices_accessor_idx);

			if (vertex_indices_accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT || local_indices_accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
				debugf(L"%S: Meshlet index accessors have wrong component type", filename.c_str());
				throw std::runtime_error("Meshlet index accessors have wrong component type");
			}

			if (vertex_indices_accessor.type != TINYGLTF_TYPE_SCALAR || local_indices_accessor.type != TINYGLTF_TYPE_SCALAR) {
				debugf(L"%S: Meshlet index accessors have wrong type", filename.c_str());
				throw std::runtime_error("Meshlet index accessors have wrong type");
			}

			if (vertex_indices_accessor.bufferView != vertex_indices_buffer_view_idx || local_indices_accessor.bufferView != local_indices_buffer_view_idx) {
				debugf(L"%S: Meshlet index accessors are not all in the same buffer view (%d, %d, expected %d, %d)", filename.c_str(), vertex_indices_accessor.bufferView, local_indices_accessor.bufferView, vertex_indices_buffer_view_idx, local_indices_buffer_view_idx);
				throw std::runtime_error("Meshlet index accessors are not all in the same buffer view");
			}

			if (vertex_indices_accessor.byteOffset % sizeof(u32) != 0) {
				debugf(L"%S: Meshlet vertex index accessor is not aligned (byteOffset: %d)", filename.c_str(), vertex_indices_accessor.byteOffset);
				throw std::runtime_error("Meshlet index accessors are not aligned");
			}

			if (local_indices_accessor.count % 3 != 0) {
				debugf(L"%S: Meshlet index accessors have wrong count (%d, expected multiple of 3)", filename.c_str(), local_indices_accessor.count);
				throw std::runtime_error("Meshlet index accessors have wrong count");
			}

			const u32 tex_idx = gltf.textures.at(gltf.materials.at(primitive.material).pbrMetallicRoughness.baseColorTexture.index).source;

			if (tex_idx < 0 || tex_idx >= gltf.images.size()) {
				debugf(L"%S: Primitive has invalid texture index %d", filename.c_str(), tex_idx);
				throw std::runtime_error("Primitive has invalid texture index");
			}

			meshlets.push_back(Meshlet{
				.vert_offset = vertex_indices_accessor.byteOffset / sizeof(u32),
				.vert_count = vertex_indices_accessor.count,
				.local_offset = local_indices_accessor.byteOffset,
				.tri_count = local_indices_accessor.count / 3,
				.tex_idx = tex_idx,
				});
		}
	}

	std::vector<MeshletVertex> vertices{
		reinterpret_cast<MeshletVertex*>(vertex_attribute_buffer.data.data() + vertex_attribute_buffer_view.byteOffset),
		reinterpret_cast<MeshletVertex*>(vertex_attribute_buffer.data.data() + vertex_attribute_buffer_view.byteOffset + vertex_attribute_buffer_view.byteLength),
	};

	for (auto& vert : vertices) {
		vert.pos = decode_point(vert.pos);
		vert.normal = decode_vector(vert.normal);
	}

	std::vector<u32> vertex_indices{
		reinterpret_cast<const u32*>(vertex_indices_buffer.data.data() + vertex_indices_buffer_view.byteOffset),
		reinterpret_cast<const u32*>(vertex_indices_buffer.data.data() + vertex_indices_buffer_view.byteOffset + vertex_indices_buffer_view.byteLength),
	};

	std::vector<u8> local_indices{
		local_indices_buffer.data.data() + local_indices_buffer_view.byteOffset,
		local_indices_buffer.data.data() + local_indices_buffer_view.byteOffset + local_indices_buffer_view.byteLength,
	};

	std::vector<std::string> texture_names;
	for (auto& image : gltf.images) {
		texture_names.push_back("gltf/" + image.uri);
	}

	return ModelReplacement{
		.name = std::move(filename),
		.meshlets = std::move(meshlets),
		.verts = std::move(vertices),
		.indices = std::move(vertex_indices),
		.local_indices = std::move(local_indices),
		.texture_file_names = std::move(texture_names),
	};
}

std::string replacement_file_name_for_texture(const UTexture* texture) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> string_converter;
	return "gltf/" + string_converter.to_bytes(texture->GetFullName()) + ".png";
}

std::optional<TextureReplacement> load_texture(const std::string& file_name) {
	int width, height, channels;
	auto data = stbi_load(file_name.c_str(), &width, &height, &channels, 4);
	if (!data) {
		debugf(L"Failed to load texture replacement %S", file_name.c_str());
		return std::nullopt;
	}
	debugf(L"Loaded texture %S, %dx%d, %d channels", file_name.c_str(), width, height, channels);
	std::vector<u8> data_vector{ data, data + width * height * 4 };
	stbi_image_free(data);
	return TextureReplacement{
		.width = static_cast<u32>(width),
		.height = static_cast<u32>(height),
		.data = std::move(data_vector),
	};
}
