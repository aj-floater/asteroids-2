#version 450

layout(set = 0, binding = 0) uniform sampler2D lightAccumulation;

layout(location = 0) in vec2 fragLocalPosition;
layout(location = 1) in vec4 fragVariation;
layout(location = 2) in vec4 fragBasis;
layout(location = 3) in vec2 fragSceneUv;

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

    float seed = fragVariation.x;
    float tintShift = fragVariation.w;
    float temperatureShift = hash12(vec2(seed * 0.023, 4.17));
    float earthyShift = hash12(vec2(seed * 0.041, -8.63));

    vec3 coolDeepShadow = vec3(0.18, 0.20, 0.25);
    vec3 warmDeepShadow = vec3(0.24, 0.21, 0.20);
    vec3 coolShadowTone = vec3(0.26, 0.29, 0.34);
    vec3 warmShadowTone = vec3(0.33, 0.30, 0.27);
    vec3 coolMidTone = vec3(0.38, 0.42, 0.47);
    vec3 warmMidTone = vec3(0.47, 0.44, 0.39);
    vec3 coolLightTone = vec3(0.58, 0.62, 0.68);
    vec3 warmLightTone = vec3(0.70, 0.67, 0.61);

    vec3 deepShadow = mix(coolDeepShadow, warmDeepShadow, temperatureShift);
    vec3 shadowTone = mix(coolShadowTone, warmShadowTone, temperatureShift);
    vec3 midTone = mix(coolMidTone, warmMidTone, temperatureShift);
    vec3 lightTone = mix(coolLightTone, warmLightTone, temperatureShift);

    vec3 earthyTint = mix(vec3(0.97, 1.00, 1.03), vec3(1.04, 1.01, 0.96), earthyShift);
    deepShadow *= earthyTint;
    shadowTone *= earthyTint;
    midTone *= earthyTint;
    lightTone *= earthyTint;

    deepShadow = mix(deepShadow, deepShadow + vec3(0.015, 0.012, 0.010), tintShift * 0.35);
    shadowTone = mix(shadowTone, shadowTone + vec3(0.012, 0.010, 0.008), tintShift * 0.30);
    midTone = mix(midTone, midTone + vec3(0.010, 0.008, 0.006), tintShift * 0.24);
    lightTone = mix(lightTone, lightTone + vec3(0.008, 0.006, 0.004), tintShift * 0.18);

    float planeMix = clamp(planeField * 0.45 + 0.5, 0.0, 1.0);
    float faceAccent = floor((planeAccent + planeDominance * 0.45) * 3.0) / 2.0;
    float shadowBlend = smoothstep(-0.96, -0.08, coarseFacing);
    float midBlend = smoothstep(-0.02, 0.30, coarseFacing + planeDominance * 0.10);
    float lightBlend = smoothstep(0.22, 0.88, coarseFacing + planeDominance * 0.14 + diffuse * 0.08);

    vec3 faceColor = mix(deepShadow, shadowTone, shadowBlend);
    faceColor = mix(faceColor, midTone, midBlend);
    faceColor = mix(faceColor, lightTone, lightBlend * 0.72);
    faceColor = mix(faceColor, midTone, planeMix * 0.10 * planeDominance);
    faceColor = mix(faceColor, lightTone, faceAccent * 0.06 * smoothstep(0.12, 0.84, coarseFacing));
    faceColor *= (0.84 + (fineDetail - 0.5) * 0.07);

    float lighting = clamp(0.54 + coarseFacing * 0.27 + diffuse * 0.13, 0.26, 0.88);
    vec3 litColor = faceColor * lighting;
    litColor += vec3(0.03, 0.04, 0.06) * smoothstep(-1.0, -0.16, -coarseFacing) * 0.55;
    litColor += lightTone * specular * (0.18 + 0.28 * smoothstep(0.0, 0.8, coarseFacing));

    float litSideRim = rim * smoothstep(0.15, 0.85, coarseFacing);
    litColor += lightTone * litSideRim * 0.08;

    vec3 brightColor = lightTone * (specular * 0.18 + litSideRim * 0.08) * smoothstep(0.20, 0.86, coarseFacing);
    float brightAlpha = clamp(specular * 0.30 + litSideRim * 0.10, 0.0, 0.12);

    vec3 gameplayLight = texture(lightAccumulation, fragSceneUv).rgb;
    float gameplayLightMask = max(max(gameplayLight.r, gameplayLight.g), gameplayLight.b);
    float surfaceResponse = mix(
        1.0,
        1.65,
        smoothstep(-0.28, 0.82, coarseFacing + diffuse * 0.22 + planeDominance * 0.10)
    );
    float highlightResponse = smoothstep(0.08, 0.95, coarseFacing + diffuse * 0.28);
    vec3 gameplayLightContribution = gameplayLight * (8.0 + 24.0 * surfaceResponse);
    litColor += gameplayLightContribution * mix(vec3(1.35), lightTone + 0.28, 0.40);

    brightColor += gameplayLight * gameplayLightMask * (8.0 + 28.0 * highlightResponse);
    brightAlpha = max(brightAlpha, clamp(gameplayLightMask * (0.85 + 1.35 * highlightResponse), 0.0, 1.0));

    outSceneColor = vec4(clamp(litColor, 0.0, 1.0), 1.0);
    outBrightColor = vec4(brightColor, brightAlpha);
}
