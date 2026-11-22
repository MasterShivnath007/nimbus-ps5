#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform vec2 uTexel;
uniform float uThreshold;
uniform float uSoftKnee;
out vec4 fragColor;

void main() {
    vec3 color = vec3(0.0);
    color += texture(uScene, vUV + uTexel * vec2(-1.0, -1.0)).rgb;
    color += texture(uScene, vUV + uTexel * vec2(1.0, -1.0)).rgb;
    color += texture(uScene, vUV + uTexel * vec2(-1.0, 1.0)).rgb;
    color += texture(uScene, vUV + uTexel * vec2(1.0, 1.0)).rgb;
    color *= 0.25;
    float luminance = max(color.r, max(color.g, color.b));
    float knee = uThreshold * uSoftKnee + 0.0001;
    float soft = clamp(luminance - uThreshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.0001);
    float contribution = max(soft, luminance - uThreshold) / max(luminance, 0.0001);
    fragColor = vec4(color * contribution, 1.0);
}
