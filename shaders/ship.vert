#version 450

layout(push_constant) uniform PushConstants {
    vec2 shipPosition;
    float shipHeading;
    vec2 worldHalfExtents;
} pc;

vec2 rotate(vec2 value, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return vec2(
        value.x * c - value.y * s,
        value.x * s + value.y * c
    );
}

void main() {
    const vec2 localPoints[6] = vec2[](
        vec2(4.0, 0.0),
        vec2(-4.0, 4.0),
        vec2(-2.0, 0.0),
        vec2(4.0, 0.0),
        vec2(-2.0, 0.0),
        vec2(-4.0, -4.0)
    );

    vec2 worldPoint = pc.shipPosition + rotate(localPoints[gl_VertexIndex], pc.shipHeading);
    vec2 clipPoint = worldPoint / pc.worldHalfExtents;
    gl_Position = vec4(clipPoint.x, -clipPoint.y, 0.0, 1.0);
}
