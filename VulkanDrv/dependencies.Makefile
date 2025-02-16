# I could probaby do this using MSBuild as well. But I know Make and don't know MSBuild, so here we are.

TINYGLTF_VERSION=2.9.5
GLM_VERSION=1.0.1

all: libs/stb_image.h libs/stb_image_write.h libs/json.hpp libs/tiny_gltf.h libs/glm/glm.hpp

libs/tinygltf-${TINYGLTF_VERSION}.zip:
	mkdir -p libs
	curl --output-dir "$(@D)" -LOJ https://github.com/syoyo/tinygltf/archive/refs/tags/v${TINYGLTF_VERSION}.zip
	
libs/glm-${GLM_VERSION}-light.zip:
	mkdir -p libs
	curl --output-dir "$(@D)" -LOJ https://github.com/g-truc/glm/releases/download/${GLM_VERSION}/glm-${GLM_VERSION}-light.zip

stb_image.h stb_image_write.h json.hpp tiny_gltf.h &: libs/tinygltf-${TINYGLTF_VERSION}.zip
	unzip -jDd libs "$<" \
		"tinygltf-${TINYGLTF_VERSION}/stb_image.h" \
		"tinygltf-${TINYGLTF_VERSION}/stb_image_write.h" \
		"tinygltf-${TINYGLTF_VERSION}/json.hpp" \
		"tinygltf-${TINYGLTF_VERSION}/tiny_gltf.h"

libs/glm/glm.hpp: libs/glm-${GLM_VERSION}-light.zip
	unzip -Dd libs "$<"