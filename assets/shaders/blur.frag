#version 330 core
in vec2 vUV;
uniform sampler2D uImage;
uniform vec2 uDirection;
uniform vec2 uTexel;
out vec4 fragColor;

void main() {
    vec3 color = texture(uImage, vUV).rgb * 0.2270270270;
    color += texture(uImage, vUV + uDirection * uTexel * 1.3846153846).rgb * 0.3162162162;
    color += texture(uImage, vUV - uDirection * uTexel * 1.3846153846).rgb * 0.3162162162;
    color += texture(uImage, vUV + uDirection * uTexel * 3.2307692308).rgb * 0.0702702703;
    color += texture(uImage, vUV - uDirection * uTexel * 3.2307692308).rgb * 0.0702702703;
    fragColor = vec4(color, 1.0);
}
