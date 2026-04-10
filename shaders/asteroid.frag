#version 450

layout(location = 0) in vec2 fragLocalPosition;
layout(location = 1) in vec4 fragVariation;
layout(location = 2) in vec4 fragBasis;

layout(location = 0) out vec4 outSceneColor;
layout(location = 1) out vec4 outBrightColor;

const float kPi = 3.14159265358979323846;
const float kTau = 6.28318530717958647692;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float value_noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

vec2 rotate_to_world(vec2 localValue, vec2 basis) {
    return vec2(
        localValue.x * basis.x - localValue.y * basis.y,
        localValue.x * basis.y + localValue.y * basis.x
    );
}

float rock_height(vec2 localUv, vec4 variation) {
    float seed = variation.x;
    float noiseScale = 0.95 + variation.y * 2.2;
    float facetDepth = variation.z;

    vec2 domain = localUv * noiseScale;
    domain += vec2(seed * 0.047, seed * 0.071);

    float coarse = value_noise(domain * vec2(0.95, 1.10));
    float mid = value_noise(domain * vec2(1.8, 2.15) + vec2(2.9, -1.7));
    float detail = value_noise(domain * vec2(3.0, 3.45) + vec2(-2.6, 1.9));
    float combined = coarse * 0.72 + mid * 0.22 + detail * 0.06;

    float quantized = floor(combined * (5.0 + facetDepth * 2.0)) / (4.0 + facetDepth * 2.0);
    return mix(combined, quantized, 0.82);
}

vec3 local_pseudo_normal(vec2 localUv, vec4 variation) {
    float eps = 0.08;
    float h = rock_height(localUv, variation);
    float hx = rock_height(localUv + vec2(eps, 0.0), variation);
    float hy = rock_height(localUv + vec2(0.0, eps), variation);
    return normalize(vec3(h - hx, h - hy, 0.72 + variation.z * 0.32));
}

float coarse_plane_field(vec2 localUv, float seed, out vec2 dominantDir, out float dominance, out float planeAccent) {
    float bestValue = -1e9;
    float secondValue = -1e9;
    dominantDir = vec2(1.0, 0.0);
    planeAccent = 0.0;

    for (int index = 0; index < 5; ++index) {
        float fi = float(index);
        float angleHash = hash12(vec2(seed * 0.031 + fi * 1.17, seed * 0.047 - fi * 0.83));
        float offsetHash = hash12(vec2(seed * 0.061 - fi * 1.31, seed * 0.029 + fi * 1.57));
        float scaleHash = hash12(vec2(seed * 0.019 + fi * 2.11, seed * 0.013 - fi * 0.61));

        float angle = angleHash * kTau;
        vec2 direction = vec2(cos(angle), sin(angle));
        float scale = mix(0.55, 1.10, scaleHash);
        float offset = (offsetHash - 0.5) * 0.9;
        float planeValue = dot(localUv, direction * scale) + offset;

        if (planeValue > bestValue) {
            secondValue = bestValue;
            bestValue = planeValue;
            dominantDir = direction;
            planeAccent = angleHash;
        } else if (planeValue > secondValue) {
            secondValue = planeValue;
        }
    }

    dominance = clamp((bestValue - secondValue) * 1.3 + 0.35, 0.0, 1.0);
    return bestValue;
}

void main() {
    vec2 rotationBasis = fragBasis.xy;
    float inverseRadius = max(fragBasis.z, 0.0001);
    vec2 localUv = fragLocalPosition * inverseRadius;
    vec3 fineNormalLocal = local_pseudo_normal(localUv, fragVariation);

    vec2 dominantDir;
    float planeDominance;
    float planeAccent;
    float planeField = coarse_plane_field(localUv, fragVariation.x, dominantDir, planeDominance, planeAccent);

    vec3 coarseNormalLocal = normalize(vec3(
        -dominantDir * mix(0.45, 1.0, planeDominance) * (0.75 + fragVariation.z * 0.35),
        1.30
    ));
    vec3 localNormal = normalize(mix(coarseNormalLocal, fineNormalLocal, 0.24));

    vec2 worldCoarseNormalXY = rotate_to_world(coarseNormalLocal.xy, rotationBasis);
    vec3 worldCoarseNormal = normalize(vec3(worldCoarseNormalXY, coarseNormalLocal.z));
    vec2 worldNormalXY = rotate_to_world(localNormal.xy, rotationBasis);
    vec3 worldNormal = normalize(vec3(worldNormalXY, localNormal.z));

    vec3 lightDir = normalize(vec3(0.72, -0.54, 0.52));
    vec3 viewDir = vec3(0.0, 0.0, 1.0);
    vec3 halfVec = normalize(lightDir + viewDir);

    float coarseFacing = dot(worldCoarseNormal, lightDir);
    float diffuse = max(dot(worldNormal, lightDir), 0.0);
    float specular = pow(max(dot(worldNormal, halfVec), 0.0), 22.0) * 0.08;
    float rim = pow(1.0 - max(dot(worldNormal, viewDir), 0.0), 3.4);
    float fineDetail = rock_height(localUv, fragVariation);

    float tintShift = fragVariation.w;
    vec3 deepShadow = mix(vec3(0.26, 0.27, 0.32), vec3(0.31, 0.31, 0.35), tintShift);
    vec3 shadowTone = mix(vec3(0.35, 0.36, 0.41), vec3(0.41, 0.42, 0.46), tintShift);
    vec3 midTone = mix(vec3(0.49, 0.50, 0.55), vec3(0.56, 0.56, 0.59), tintShift);
    vec3 lightTone = mix(vec3(0.73, 0.74, 0.78), vec3(0.84, 0.83, 0.79), tintShift);

    float planeMix = clamp(planeField * 0.45 + 0.5, 0.0, 1.0);
    float faceAccent = floor((planeAccent + planeDominance * 0.45) * 3.0) / 2.0;
    float shadowBlend = smoothstep(-0.92, -0.18, coarseFacing);
    float midBlend = smoothstep(-0.08, 0.34, coarseFacing + planeDominance * 0.10);
    float lightBlend = smoothstep(0.22, 0.88, coarseFacing + planeDominance * 0.14 + diffuse * 0.08);

    vec3 faceColor = mix(deepShadow, shadowTone, shadowBlend);
    faceColor = mix(faceColor, midTone, midBlend);
    faceColor = mix(faceColor, lightTone, lightBlend * 0.78);
    faceColor = mix(faceColor, midTone, planeMix * 0.10 * planeDominance);
    faceColor = mix(faceColor, lightTone, faceAccent * 0.06 * smoothstep(0.12, 0.84, coarseFacing));
    faceColor *= 0.96 + (fineDetail - 0.5) * 0.08;

    float lighting = clamp(0.64 + coarseFacing * 0.28 + diffuse * 0.16, 0.40, 0.96);
    vec3 litColor = faceColor * lighting;
    litColor += vec3(0.05, 0.06, 0.08) * smoothstep(-1.0, -0.16, -coarseFacing) * 0.9;
    litColor += lightTone * specular * (0.25 + 0.35 * smoothstep(0.0, 0.8, coarseFacing));

    float litSideRim = rim * smoothstep(0.15, 0.85, coarseFacing);
    litColor += lightTone * litSideRim * 0.10;

    vec3 brightColor = lightTone * (specular * 0.25 + litSideRim * 0.10) * smoothstep(0.20, 0.86, coarseFacing);
    float brightAlpha = clamp(specular * 0.40 + litSideRim * 0.12, 0.0, 0.16);

    outSceneColor = vec4(clamp(litColor, 0.0, 1.0), 1.0);
    outBrightColor = vec4(brightColor, brightAlpha);
}
