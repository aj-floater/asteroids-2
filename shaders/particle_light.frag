#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragLocalCoord;
layout(location = 0) out vec4 outLight;

void main() {
    float radial = max(0.0, 1.0 - length(fragLocalCoord));
    float softness = radial * radial * radial;
    vec3 warmGlow = fragColor.rgb * softness * 0.02;
    outLight = vec4(warmGlow, 1.0);
}
