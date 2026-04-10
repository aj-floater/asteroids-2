#version 450

layout(location = 0) in vec2 inPosition;

layout(push_constant) uniform PushConstants {
    vec2 shipPosition;
    float shipHeading;
    vec2 worldHalfExtents;
} pc;

void main() {
    vec2 clipPoint = inPosition / pc.worldHalfExtents;
    gl_Position = vec4(clipPoint.x, -clipPoint.y, 0.0, 1.0);
}
