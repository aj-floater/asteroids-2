#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inSize;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragLocalCoord;

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
    const float lightRadiusScale = 12.0;

    vec2 localCoord = quad_corner(gl_VertexIndex);
    vec2 clipCenter = inPosition / worldHalfExtents;
    vec2 clipOffset = localCoord * vec2(
        (inSize * lightRadiusScale) / worldHalfExtents.x,
        (inSize * lightRadiusScale) / worldHalfExtents.y
    );

    fragColor = inColor;
    fragLocalCoord = localCoord;
    gl_Position = vec4(clipCenter.x + clipOffset.x, -(clipCenter.y + clipOffset.y), 0.0, 1.0);
}
