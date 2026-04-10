#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inLocalPosition;
layout(location = 2) in vec4 inVariation;
layout(location = 3) in vec4 inBasis;

layout(push_constant) uniform PushConstants {
    vec2 shipPosition;
    float shipHeading;
    vec2 worldHalfExtents;
} pc;

layout(location = 0) out vec2 fragLocalPosition;
layout(location = 1) out vec4 fragVariation;
layout(location = 2) out vec4 fragBasis;

void main() {
    fragLocalPosition = inLocalPosition;
    fragVariation = inVariation;
    fragBasis = inBasis;

    vec2 clipPoint = inPosition / pc.worldHalfExtents;
    gl_Position = vec4(clipPoint.x, -clipPoint.y, 0.0, 1.0);
}
