#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec4 fragParams;

layout(location = 0) out vec4 outColor;

void main() {
    if (fragColor.a <= 0.0001) {
        discard;
    }

    float emissiveStrength = fragParams.x;
    vec3 emissiveColor = fragColor.rgb * emissiveStrength;
    outColor = vec4(emissiveColor, fragColor.a);
}
