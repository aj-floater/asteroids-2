#version 450

layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outBrightColor;

void main() {
    outSceneColor = vec4(0.34, 0.34, 0.36, 1.0);
    outBrightColor = vec4(0.0);
}
