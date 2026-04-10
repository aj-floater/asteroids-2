#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragLocalCoord;
layout(location = 2) in vec4 fragParams;
layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outBrightColor;

void main() {
    float isRectangle = fragParams.x;
    float glowIntensity = fragParams.y;
    float bloomIntensity = fragParams.z;

    float coreDistance = mix(
        max(abs(fragLocalCoord.x), abs(fragLocalCoord.y)),
        max(abs(fragLocalCoord.x) / 0.9, abs(fragLocalCoord.y) / 0.16),
        isRectangle
    );
    float coreThreshold = mix(0.285, 1.0, isRectangle);
    float coreMask = 1.0 - step(coreThreshold, coreDistance);

    float haloDistance = mix(
        length(fragLocalCoord),
        length(vec2(fragLocalCoord.x / 1.15, fragLocalCoord.y / 0.28)),
        isRectangle
    );
    float radial = max(0.0, 1.0 - haloDistance);
    float haloStrength = radial * radial * radial * glowIntensity;

    vec3 haloColor = fragColor.rgb * haloStrength * mix(0.26, 0.82, isRectangle);
    float haloAlpha = fragColor.a * haloStrength * mix(0.2, 0.62, isRectangle);

    vec3 rectangleCoreTint = vec3(0.0, 0.68, 0.31);
    vec3 coreColor = mix(fragColor.rgb, rectangleCoreTint, isRectangle) * coreMask;
    float coreAlpha = fragColor.a * coreMask;

    vec3 sceneRgb = haloColor + coreColor;
    float sceneAlpha = clamp(haloAlpha + coreAlpha, 0.0, 1.0);

    outSceneColor = vec4(sceneRgb, sceneAlpha);
    outBrightColor = vec4(
        fragColor.rgb * haloStrength * mix(0.52, 2.8, isRectangle) * bloomIntensity,
        fragColor.a * haloStrength * mix(0.65, 2.4, isRectangle) * bloomIntensity
    );
}
