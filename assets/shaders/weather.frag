#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;

uniform sampler2D uData;
uniform int uLayer;             // 0 temperature, 1 humidity, 2 pressure
uniform float uTempMin;
uniform float uTempMax;
uniform float uHumidityMin;
uniform float uHumidityMax;
uniform float uPressureMin;
uniform float uPressureMax;
uniform float uOpacity;
uniform float uContourStep;
uniform float uContourAlpha;
uniform vec3 uSunDir;
uniform float uTime;

out vec4 fragColor;

vec3 temp_color(float t) {
    vec3 c0 = vec3(0.10, 0.22, 0.65);
    vec3 c1 = vec3(0.06, 0.72, 0.96);
    vec3 c2 = vec3(0.24, 0.92, 0.62);
    vec3 c3 = vec3(0.94, 0.90, 0.34);
    vec3 c4 = vec3(0.99, 0.54, 0.15);
    vec3 c5 = vec3(0.86, 0.12, 0.18);
    t = clamp(t, 0.0, 1.0);
    if (t < 0.2) return mix(c0, c1, t / 0.2);
    if (t < 0.4) return mix(c1, c2, (t - 0.2) / 0.2);
    if (t < 0.6) return mix(c2, c3, (t - 0.4) / 0.2);
    if (t < 0.8) return mix(c3, c4, (t - 0.6) / 0.2);
    return mix(c4, c5, (t - 0.8) / 0.2);
}

vec3 humidity_color(float t) {
    t = clamp(t, 0.0, 1.0);
    return mix(mix(vec3(0.32, 0.12, 0.42), vec3(0.05, 0.48, 0.86), smoothstep(0.0, 0.55, t)),
               vec3(0.50, 1.0, 0.85), smoothstep(0.55, 1.0, t));
}

vec3 pressure_color(float t) {
    t = clamp(t, 0.0, 1.0);
    return mix(mix(vec3(0.42, 0.10, 0.58), vec3(0.08, 0.54, 0.72), smoothstep(0.0, 0.5, t)),
               vec3(0.80, 0.74, 0.46), smoothstep(0.5, 1.0, t));
}

void main() {
    vec4 data = texture(uData, vUV);
    float raw = uLayer == 0 ? data.r : (uLayer == 1 ? data.g : data.b);
    // Decode the stored normalised channel back to real units, then map it
    // across the current field's range.  The legend and the colours therefore
    // describe the same values, and pressure contours can use true hPa.
    float value = uLayer == 0 ? mix(-60.0, 60.0, raw)
                              : (uLayer == 1 ? raw * 100.0 : mix(880.0, 1060.0, raw));
    float lo = uLayer == 0 ? uTempMin : (uLayer == 1 ? uHumidityMin : uPressureMin);
    float hi = uLayer == 0 ? uTempMax : (uLayer == 1 ? uHumidityMax : uPressureMax);
    float norm = clamp((value - lo) / max(0.0001, hi - lo), 0.0, 1.0);

    vec3 color;
    float alpha = uOpacity;
    if (uLayer == 0) {
        color = temp_color(norm);
    } else if (uLayer == 1) {
        color = humidity_color(norm);
    } else {
        color = pressure_color(norm);
        // Isobars.  At planetary scale they whisper; close to the ground they
        // become a real meteorological overlay.
        float d = abs(fract((value - lo) / uContourStep + 0.5) - 0.5) * uContourStep;
        float line = 1.0 - smoothstep(0.10, 0.34, d);
        color = mix(color, vec3(0.92, 0.97, 1.0), line * 0.62);
        alpha += line * uContourAlpha;
    }

    // A fine grain keeps the large smooth fields from banding on a TV.
    float grain = fract(sin(dot(vUV * 1483.0 + uTime * 0.01, vec2(12.9898, 78.233))) * 43758.5453);
    color += (grain - 0.5) * 0.012;

    // Dim the layer slightly on the night side but keep it legible.
    float ndl = dot(normalize(vNormal), normalize(uSunDir));
    float night = 1.0 - smoothstep(-0.15, 0.25, ndl);
    color *= 1.0 - night * 0.18;
    alpha *= 1.0 - night * 0.12;

    fragColor = vec4(color, alpha);
}
