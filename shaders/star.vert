#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inParams;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragLocalCoord;
layout(location = 2) out vec4 fragParams;

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

    vec2 localCoord = quad_corner(gl_VertexIndex);
    vec2 worldOffset = localCoord * inParams.x;
    vec2 worldPoint = inPosition + worldOffset;
    vec2 clipPoint = worldPoint / worldHalfExtents;

    fragColor = inColor;
    fragLocalCoord = localCoord;
    fragParams = inParams;
    gl_Position = vec4(clipPoint.x, -clipPoint.y, 0.0, 1.0);
}
