#version 450
#extension GL_EXT_nonuniform_qualifier: enable


layout(binding = 6) uniform sampler texSampler;
layout(binding = 7) uniform texture2D textures[];

layout(location = 0) in vec3 inNormal;
layout(location = 1) nonuniformEXT flat in uint inTexIndex;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in float inWhite;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 N = normalize(inNormal);
	vec3 L = normalize(vec3(1.0, 1.0, 2.0));
	float lightness = dot(N, L) * 0.5 + 0.5;
	vec4 color = texture(sampler2D(textures[inTexIndex], texSampler), inTexCoord);
	if (inWhite == 0 && color.a < 1) {
		discard;
	}

	outColor = vec4(mix(color.xyz * 2, vec3(1,1,1), inWhite), mix(color.a, 1, inWhite));
}