#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inSize;

layout(location = 0) out vec4 fragColor;

vec2 quad_corner(uint index) {
    const vec2 corners[4] = vec2[](
        vec2(-1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0, -1.0),
        vec2( 1.0,  1.0)
    );
    return corners[index];
}

void main() {
    const vec2 worldHalfExtents = vec2(100.0, 75.0);
    vec2 clipCenter = inPosition / worldHalfExtents;
    vec2 clipOffset = quad_corner(gl_VertexIndex) * vec2(inSize / worldHalfExtents.x, inSize / worldHalfExtents.y);

    fragColor = inColor;
    gl_Position = vec4(clipCenter.x + clipOffset.x, -(clipCenter.y + clipOffset.y), 0.0, 1.0);
}
