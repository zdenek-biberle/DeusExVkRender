#ifndef GLTF_H
#define GLTF_H

#include "Precomp.h"
#include "tinygltf.h"
#include "types.h"

void save_model_to_gltf(const UModel* model);

struct ModelReplacement {
	std::string name;
	std::vector<Meshlet> meshlets;
	std::vector<MeshletVertex> verts;
	std::vector<u32> indices;
	std::vector<u8> local_indices;

	// TODO: we should have proper materials here
	std::vector<std::string> texture_file_names;
};

std::optional<ModelReplacement> load_replacement_for_model(const UModel* model);

struct TextureReplacement {
	u32 width;
	u32 height;
	std::vector<u8> data; // RGBA data
};

std::string replacement_file_name_for_texture(const UTexture* texture);

std::optional<TextureReplacement> load_texture(const std::string& file_name);

#endif
