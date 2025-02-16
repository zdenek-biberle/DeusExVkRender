#include "Precomp.h"
#include "gltf.h"
#include "tinygltf.h"
#include <codecvt>

struct output_vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texcoord;
};

typedef UINT uint;

void save_model_to_gltf(const UModel* model) {
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
	//const auto texture_buffer_idx = gltf.buffers.size();
	//gltf.buffers.emplace_back();

	// Create buffer views.
	// We gotta fill in the byte length later.
	tinygltf::BufferView vertex_buffer_view;
	vertex_buffer_view.buffer = vertex_buffer_idx;
	vertex_buffer_view.byteStride = sizeof(output_vertex);
	const auto vertex_data_buffer_view_index = gltf.bufferViews.size();
	gltf.bufferViews.push_back(std::move(vertex_buffer_view));
	tinygltf::BufferView index_buffer_view;
	index_buffer_view.buffer = index_buffer_idx;
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
	std::vector<uint> indices;

	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> string_converter;
	tinygltf::Mesh mesh;
	FBox bounding_box;
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
			const auto normal_fixed = FVector{ normal.X, normal.Z, normal.Y };
			const auto first_vertex_idx_for_node = vertices.size();

			for (int j = 0; j < node.NumVertices; j++) {
				const auto point = model->Points(model->Verts(node.iVertPool + j).pVertex);
				const FVector point_fixed{ point.X, point.Z, point.Y };
				bounding_box += point_fixed;
				vertices.push_back(output_vertex{
					{ point_fixed.X, point_fixed.Y, point_fixed.Z },
					{ normal_fixed.X, normal_fixed.Y, normal_fixed.Z },
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
		indices_accessor.byteOffset = first_index_idx * sizeof(uint);
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

	/*tinygltf::Node node;
	node.mesh = 0;
	gltf.nodes.push_back(std::move(node));

	tinygltf::Scene scene;
	scene.name = string_converter.to_bytes(model->GetFullName());
	scene.nodes.push_back(0);
	gltf.scenes.push_back(std::move(scene));

	gltf.defaultScene = 0;*/

	tinygltf::TinyGLTF gltf_writer;
	auto filename = "gltf/" + string_converter.to_bytes(model->GetFullName()) + ".gltf";
	if (!gltf_writer.WriteGltfSceneToFile(&gltf, filename, false, false, true, false)) {
		throw std::runtime_error("Failed to write gltf file");
	}
}