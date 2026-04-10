#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneTexture;
layout(set = 0, binding = 1) uniform sampler2D bloomTexture;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 scene = texture(sceneTexture, fragUv).rgb;
    vec3 bloom = texture(bloomTexture, fragUv).rgb;
    outColor = vec4(min(scene + bloom * 0.5, vec3(1.0)), 1.0);
}
