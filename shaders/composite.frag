#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneTexture;
layout(set = 0, binding = 1) uniform sampler2D bloomTexture;

layout(push_constant) uniform CompositePushConstants {
    float sceneMix;
    float bloomStrength;
    float dimFactor;
    float playableUvMinX;
    float playableUvMinY;
    float playableUvMaxX;
    float playableUvMaxY;
    float marginDim;
    float marginTint;
} pc;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 scene = texture(sceneTexture, fragUv).rgb;
    vec3 bloom = texture(bloomTexture, fragUv).rgb;
    vec3 color = scene * pc.sceneMix + bloom * pc.bloomStrength;

    float horizontalMargin = max(pc.playableUvMinX, 1.0 - pc.playableUvMaxX);
    float verticalMargin = max(pc.playableUvMinY, 1.0 - pc.playableUvMaxY);
    float horizontalStrength = smoothstep(0.0, 0.14, horizontalMargin);
    float verticalStrength = smoothstep(0.0, 0.14, verticalMargin);

    float leftEdge = fragUv.x < pc.playableUvMinX
        ? 1.0 - (pc.playableUvMinX - fragUv.x) / max(pc.playableUvMinX, 0.0001)
        : 0.0;
    float rightEdge = fragUv.x > pc.playableUvMaxX
        ? 1.0 - (fragUv.x - pc.playableUvMaxX) / max(1.0 - pc.playableUvMaxX, 0.0001)
        : 0.0;
    float topEdge = fragUv.y < pc.playableUvMinY
        ? 1.0 - (pc.playableUvMinY - fragUv.y) / max(pc.playableUvMinY, 0.0001)
        : 0.0;
    float bottomEdge = fragUv.y > pc.playableUvMaxY
        ? 1.0 - (fragUv.y - pc.playableUvMaxY) / max(1.0 - pc.playableUvMaxY, 0.0001)
        : 0.0;
    leftEdge *= horizontalStrength;
    rightEdge *= horizontalStrength;
    topEdge *= verticalStrength;
    bottomEdge *= verticalStrength;
    float edgeMask = clamp(
        max(max(leftEdge, rightEdge), max(topEdge, bottomEdge)),
        0.0,
        1.0
    );
    edgeMask = pow(smoothstep(0.0, 1.0, edgeMask), 0.9);

    float innerEdgeDistanceX = min(
        fragUv.x - pc.playableUvMinX,
        pc.playableUvMaxX - fragUv.x
    );
    float innerEdgeDistanceY = min(
        fragUv.y - pc.playableUvMinY,
        pc.playableUvMaxY - fragUv.y
    );
    float innerFeatherX = mix(0.0, 0.018, horizontalStrength);
    float innerFeatherY = mix(0.0, 0.018, verticalStrength);
    float innerEdgeMask = 0.0;
    if (fragUv.x >= pc.playableUvMinX && fragUv.x <= pc.playableUvMaxX &&
        fragUv.y >= pc.playableUvMinY && fragUv.y <= pc.playableUvMaxY) {
        float innerEdgeMaskX = 0.0;
        float innerEdgeMaskY = 0.0;
        if (horizontalStrength > 0.0001) {
            innerEdgeMaskX = 1.0 - clamp(innerEdgeDistanceX / max(innerFeatherX, 0.0001), 0.0, 1.0);
            innerEdgeMaskX = smoothstep(0.0, 1.0, innerEdgeMaskX) * horizontalStrength;
        }
        if (verticalStrength > 0.0001) {
            innerEdgeMaskY = 1.0 - clamp(innerEdgeDistanceY / max(innerFeatherY, 0.0001), 0.0, 1.0);
            innerEdgeMaskY = smoothstep(0.0, 1.0, innerEdgeMaskY) * verticalStrength;
        }
        innerEdgeMask = max(innerEdgeMaskX, innerEdgeMaskY);
    }

    float grayMask = max(edgeMask, innerEdgeMask);
    float dimMask = max(edgeMask, innerEdgeMask * 0.55);

    color *= mix(1.0, 1.0 - pc.marginDim, dimMask);

    vec3 grayOverlay = vec3(0.12) * grayMask * pc.marginTint;
    color += grayOverlay;

    outColor = vec4(min(color * pc.dimFactor, vec3(1.0)), 1.0);
}
