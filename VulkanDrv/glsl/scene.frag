#version 450
#extension GL_EXT_nonuniform_qualifier: enable

struct Light {
	vec3 position;
	int isLight;
	vec3 color;
	int pad;
};

layout(std430, binding = 6) readonly buffer LightBuffer{ Light lights[]; };
layout(binding = 7) uniform sampler texSampler;
layout(binding = 8) uniform texture2D textures[];

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorldPosition;
layout(location = 2) nonuniformEXT flat in uint inTexIndex;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) nonuniformEXT flat in uint inLightsIndex;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 N = normalize(inNormal);
	vec4 color = texture(sampler2D(textures[inTexIndex], texSampler), inTexCoord);
	if (color.a < 1) {
		discard;
	}

	vec3 lightMap = vec3(1,1,1);
	if (inLightsIndex != ~0u) {
		Light light = lights[inLightsIndex];
		lightMap = vec3(0, 0, 0);
		for (uint i = inLightsIndex; lights[i].isLight != 0; i++) {
			Light light = lights[i];
			vec3 toLight = light.position - inWorldPosition;
			vec3 lightDir = normalize(toLight);  
			float distanceSquared = dot(toLight / 10, toLight / 10);
			float diffuse = max(dot(N, lightDir), 0.0) / (0.1 + distanceSquared);
			lightMap += diffuse * light.color;
		}
	}

	outColor = vec4(color.xyz * lightMap * 2, color.a);
}