#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inParams;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragParams;

void main() {
    fragColor = inColor;
    fragParams = inParams;
    gl_Position = vec4(inPosition.x, -inPosition.y, 0.0, 1.0);
}
