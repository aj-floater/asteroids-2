#version 450

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform BlurPushConstants {
    vec2 texelOffset;
} pc;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = texture(inputTexture, fragUv).rgb * 0.227027;
    color += texture(inputTexture, fragUv + pc.texelOffset * 1.384615).rgb * 0.316216;
    color += texture(inputTexture, fragUv - pc.texelOffset * 1.384615).rgb * 0.316216;
    color += texture(inputTexture, fragUv + pc.texelOffset * 3.230769).rgb * 0.070270;
    color += texture(inputTexture, fragUv - pc.texelOffset * 3.230769).rgb * 0.070270;
    outColor = vec4(color, 1.0);
}
