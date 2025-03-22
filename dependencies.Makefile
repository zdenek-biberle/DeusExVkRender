# I could probaby do this using MSBuild as well. But I know Make and don't know MSBuild, so here we are.

TINYGLTF_VERSION=2.9.5
TINYGLTF_ZIP=libs/tinygltf-${TINYGLTF_VERSION}.zip
TINYGLTF_FILES=libs/tinygltf/stb_image.h \
	libs/tinygltf/stb_image_write.h \
	libs/tinygltf/json.hpp \
	libs/tinygltf/tiny_gltf.h

GLM_VERSION=1.0.1
GLM_ZIP=libs/glm-${GLM_VERSION}-light.zip
GLM_FILES=libs/glm/glm.hpp

MESHOPTIMIZER_VERSION=0.22
MESHOPTIMIZER_ZIP=libs/meshoptimizer-${MESHOPTIMIZER_VERSION}.zip
MESHOPTIMIZER_FILES=libs/meshoptimizer/meshoptimizer.h \
	libs/meshoptimizer/clusterizer.cpp

all: ${TINYGLTF_FILES} ${GLM_FILES} ${MESHOPTIMIZER_FILES}

${TINYGLTF_ZIP}:
	mkdir -p libs
	curl --clobber --output-dir "$(@D)" -LOJ https://github.com/syoyo/tinygltf/archive/refs/tags/v${TINYGLTF_VERSION}.zip
	
${GLM_ZIP}:
	mkdir -p libs
	curl --clobber --output-dir "$(@D)" -LOJ https://github.com/g-truc/glm/releases/download/${GLM_VERSION}/glm-${GLM_VERSION}-light.zip

${MESHOPTIMIZER_ZIP}:
	mkdir -p libs
	curl --clobber --output-dir "$(@D)" -LOJ https://github.com/zeux/meshoptimizer/archive/refs/tags/v${MESHOPTIMIZER_VERSION}.zip

${TINYGLTF_FILES} &: ${TINYGLTF_ZIP}
	unzip -jDDod libs/tinygltf "$<" \
		"tinygltf-${TINYGLTF_VERSION}/stb_image.h" \
		"tinygltf-${TINYGLTF_VERSION}/stb_image_write.h" \
		"tinygltf-${TINYGLTF_VERSION}/json.hpp" \
		"tinygltf-${TINYGLTF_VERSION}/tiny_gltf.h"

${GLM_FILES} &: ${GLM_ZIP}
	unzip -DDod libs "$<"

${MESHOPTIMIZER_FILES} &: ${MESHOPTIMIZER_ZIP}
	mkdir -p libs/meshoptimizer
	unzip -jDDod libs/meshoptimizer "$<" \
		"meshoptimizer-${MESHOPTIMIZER_VERSION}/src/meshoptimizer.h" \
		"meshoptimizer-${MESHOPTIMIZER_VERSION}/src/clusterizer.cpp"