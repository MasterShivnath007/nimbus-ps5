#version 330 core
in vec2 vUV;
in vec3 vWorld;
in vec3 vNormal;

uniform sampler2D uTile;
uniform sampler2D uOverlay;
uniform sampler2D uNightTex;
uniform vec3 uSunDir;
uniform vec3 uCamPos;
uniform float uNightMix;
uniform float uNightStrength;
uniform float uNightLights;
uniform float uOverlayMix;
uniform float uOverlayMode;
uniform float uSaturation;
uniform float uBrightness;

out vec4 fragColor;

vec3 srgb_to_linear(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }

void main() {
    if (uOverlayMode > 0.5) {
        vec4 over = texture(uOverlay, vUV);
        float alpha = over.a * uOverlayMix;
        if (alpha < 0.01) discard;
        fragColor = vec4(srgb_to_linear(over.rgb), alpha);
        return;
    }

    vec3 tex = srgb_to_linear(texture(uTile, vUV).rgb);
    float luma = dot(tex, vec3(0.2126, 0.7152, 0.0722));
    tex = mix(vec3(luma), tex, uSaturation);
    tex *= uBrightness;

    float ndl = dot(normalize(vNormal), normalize(uSunDir));
    float daylight = smoothstep(-0.16, 0.22, ndl);
    float shade = mix(1.0, 0.42 + 0.72 * max(ndl, 0.0), 0.45);
    vec3 color = tex * shade;
    // Keep some map readability at night instead of turning the street map black.
    color = mix(color, color * vec3(0.34, 0.43, 0.62), uNightMix * uNightStrength);
    color += vec3(0.026, 0.048, 0.085) * uNightMix * 0.30;

    vec3 up = normalize(vNormal);
    float fres = pow(1.0 - max(dot(up, normalize(uCamPos - vWorld)), 0.0), 4.0);
    color += vec3(0.10, 0.28, 0.55) * fres * 0.10;

    // City lights only where the equirectangular night map has energy.
    vec2 eq_uv = vec2(atan(vNormal.y, vNormal.x) / 6.28318530718 + 0.5,
                      0.5 - asin(clamp(vNormal.z, -1.0, 1.0)) / 3.14159265359);
    vec3 lights = texture(uNightTex, eq_uv).rgb;
    color += lights * uNightMix * uNightLights;

    fragColor = vec4(color, 1.0);
}
