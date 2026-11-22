#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomStrength;
uniform float uExposure;
uniform vec2 uResolution;
uniform float uVignette;
out vec4 fragColor;

vec3 aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 scene = texture(uScene, vUV).rgb;
    vec3 bloom = texture(uBloom, vUV).rgb;
    vec3 color = scene + bloom * uBloomStrength;
    color = aces(color * uExposure);
    // Slight filmic contrast and a cool shadow lift.
    color = pow(color, vec3(0.94));
    vec2 centered = vUV * 2.0 - 1.0;
    float vignette = 1.0 - uVignette * smoothstep(0.35, 1.45, dot(centered, centered));
    color *= vignette;
    color = mix(vec3(dot(color, vec3(0.2126, 0.7152, 0.0722))), color, 1.06);
    // Dither to hide 8-bit gradient banding.
    float dither = fract(sin(dot(vUV * uResolution, vec2(12.9898, 78.233))) * 43758.5453);
    color += (dither - 0.5) / 255.0;
    fragColor = vec4(color, 1.0);
}
