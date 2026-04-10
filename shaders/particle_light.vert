#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inParams0;
layout(location = 3) in vec4 inParams1;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragLocalCoord;
layout(location = 2) out float fragLightIntensity;

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
    float lightRadiusScale = max(inParams1.x * 3.4, 1.2);
    vec2 localShapeOffset = vec2(
        localCoord.x * inParams0.x * lightRadiusScale,
        localCoord.y * inParams0.x * inParams0.z * lightRadiusScale
    );
    float c = cos(inParams0.y);
    float s = sin(inParams0.y);
    vec2 rotatedCoord = vec2(
        localShapeOffset.x * c - localShapeOffset.y * s,
        localShapeOffset.x * s + localShapeOffset.y * c
    );

    vec2 clipCenter = inPosition / worldHalfExtents;
    vec2 clipOffset = vec2(
        rotatedCoord.x / worldHalfExtents.x,
        rotatedCoord.y / worldHalfExtents.y
    );

    fragColor = inColor;
    fragLocalCoord = localCoord;
    fragLightIntensity = inParams1.w;
    gl_Position = vec4(clipCenter.x + clipOffset.x, -(clipCenter.y + clipOffset.y), 0.0, 1.0);
}
