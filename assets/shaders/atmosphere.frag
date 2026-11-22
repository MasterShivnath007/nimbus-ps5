#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;

uniform vec3 uSunDir;
uniform vec3 uCamPos;
uniform float uNightBoost;

out vec4 fragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorld);
    vec3 L = normalize(uSunDir);
    float rim = pow(1.0 - max(dot(N, V), 0.0), 2.7);
    float facing = smoothstep(-0.35, 0.55, dot(N, L));
    float sun_set = smoothstep(-0.2, 0.35, dot(N, L));
    vec3 color = mix(vec3(0.055, 0.16, 0.38), vec3(0.20, 0.52, 1.0), sun_set);
    color += vec3(1.0, 0.34, 0.10) * pow(max(0.0, 1.0 - abs(dot(N, L))), 4.0) * 0.16;
    float alpha = rim * (0.12 + 0.88 * facing) * 0.52;
    alpha *= uNightBoost;
    fragColor = vec4(color * alpha, alpha);
}
