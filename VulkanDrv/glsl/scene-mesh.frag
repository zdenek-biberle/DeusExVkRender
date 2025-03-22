#version 450
#extension GL_EXT_nonuniform_qualifier: enable

layout(binding = 4) uniform sampler texSampler;
layout(binding = 5) uniform texture2D textures[];

layout(location = 0) in vec3 inNormal;
layout(location = 1) nonuniformEXT flat in uint inTexIndex;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 N = normalize(inNormal);
	vec4 color = texture(sampler2D(textures[inTexIndex], texSampler), inTexCoord);
	if (color.a < 1) {
		discard;
	}

	outColor = vec4(color.xyz * 2, color.a);
}