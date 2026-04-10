#version 450

layout(push_constant) uniform StarPushConstants {
    float elapsedTimeSeconds;
} pc;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragLocalCoord;
layout(location = 2) in vec4 fragParams;
layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outBrightColor;

void main() {
    float size = fragParams.x;
    float baseBrightness = fragParams.y;
    float twinkleAmplitude = fragParams.z;
    float twinkleSpeed = fragParams.w;
    float twinklePhase = fragColor.a;

    float twinkle = sin(pc.elapsedTimeSeconds * twinkleSpeed + twinklePhase);
    float brightness = baseBrightness + twinkle * twinkleAmplitude;
    float glow = max(0.0, 1.0 - length(fragLocalCoord));
    float softness = glow * glow * mix(1.6, 2.1, clamp(size * 2.0, 0.0, 1.0));
    float alpha = clamp(softness * brightness, 0.0, 1.0);

    outSceneColor = vec4(fragColor.rgb * brightness, alpha);
    outBrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}
