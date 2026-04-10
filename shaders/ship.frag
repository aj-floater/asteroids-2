#version 450

layout(set = 0, binding = 0) uniform sampler2D lightAccumulation;

layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outBrightColor;

void main() {
    vec2 lightUv = gl_FragCoord.xy / vec2(textureSize(lightAccumulation, 0));
    vec3 emittedLight = texture(lightAccumulation, lightUv).rgb;
    vec3 baseColor = vec3(0.541, 0.082, 0.220);
    vec3 litColor = baseColor + emittedLight * vec3(2.6, 1.25, 0.38);
    float bloomMask = max(max(emittedLight.r, emittedLight.g), emittedLight.b);

    outSceneColor = vec4(litColor, 1.0);
    outBrightColor = vec4(litColor * max(bloomMask - 0.12, 0.0) * 1.7, 1.0);
}
