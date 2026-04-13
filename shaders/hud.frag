#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec4 fragParams;

layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outBrightColor;

void main() {
    if (fragColor.a <= 0.0001) {
        discard;
    }

    float emissiveStrength = fragParams.x;
    float sceneAlphaScale = fragParams.y;
    vec3 emissiveColor = fragColor.rgb * fragColor.a * emissiveStrength;
    float sceneAlpha = fragColor.a * sceneAlphaScale;

    outSceneColor = vec4(fragColor.rgb, sceneAlpha);
    outBrightColor = vec4(emissiveColor, 0.0);
}
