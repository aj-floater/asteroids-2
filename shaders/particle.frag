#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragLocalCoord;
layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outBrightColor;

void main() {
    const float coreHalfWidth = 0.285;

    float squareDistance = max(abs(fragLocalCoord.x), abs(fragLocalCoord.y));
    float coreMask = 1.0 - step(coreHalfWidth, squareDistance);

    float radial = max(0.0, 1.0 - length(fragLocalCoord));
    float haloStrength = radial * radial * radial;

    vec3 haloColor = fragColor.rgb * haloStrength * 0.26;
    float haloAlpha = fragColor.a * haloStrength * 0.2;

    vec3 coreColor = fragColor.rgb * coreMask;
    float coreAlpha = fragColor.a * coreMask;

    vec3 sceneRgb = haloColor + coreColor;
    float sceneAlpha = clamp(haloAlpha + coreAlpha, 0.0, 1.0);

    outSceneColor = vec4(sceneRgb, sceneAlpha);
    outBrightColor = vec4(fragColor.rgb * haloStrength * 0.52, fragColor.a * haloStrength * 0.65);
}
