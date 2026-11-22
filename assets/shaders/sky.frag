#version 330 core
in vec2 vNdc;

uniform vec3 uCameraPos;
uniform vec3 uLocalUp;
uniform vec3 uForward;
uniform vec3 uRight;
uniform vec3 uUp;
uniform float uTanHalfFov;
uniform float uAspect;
uniform vec3 uSunEci;
uniform vec3 uMoonEci;
uniform vec3 uSunEcef;
uniform float uGmst;
uniform float uSurface;      // 0 = space, 1 = standing on the ground
uniform float uStarAlpha;
uniform float uTime;
uniform sampler2D uMoonTex;

out vec4 fragColor;

const float PI = 3.14159265359;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float hash31(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z);
}

float value_noise3(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash31(i + vec3(0, 0, 0));
    float n100 = hash31(i + vec3(1, 0, 0));
    float n010 = hash31(i + vec3(0, 1, 0));
    float n110 = hash31(i + vec3(1, 1, 0));
    float n001 = hash31(i + vec3(0, 0, 1));
    float n101 = hash31(i + vec3(1, 0, 1));
    float n011 = hash31(i + vec3(0, 1, 1));
    float n111 = hash31(i + vec3(1, 1, 1));
    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.55;
    for (int i = 0; i < 5; ++i) {
        value += amplitude * value_noise3(p);
        p = p * 2.02 + vec3(13.0, 7.0, 19.0);
        amplitude *= 0.48;
    }
    return value;
}

vec3 eci_from_ecef(vec3 v) {
    float c = cos(uGmst);
    float s = sin(uGmst);
    return vec3(c * v.x - s * v.y, s * v.x + c * v.y, v.z);
}

vec3 space_color(vec3 ray_eci, bool visible) {
    // A soft, dust-reddened Milky Way band and faint airglow.  The band is a
    // fixed plane in celestial coordinates; the stars themselves are points.
    vec3 galactic_normal = normalize(vec3(0.31, 0.71, 0.63));
    float band = exp(-pow(dot(ray_eci, galactic_normal) / 0.24, 2.0));
    float dust = fbm(ray_eci * 2.7 + vec3(0.4, 0.1, 0.7));
    float clouds = smoothstep(0.35, 0.78, dust) * band;
    vec3 milky = mix(vec3(0.035, 0.048, 0.080), vec3(0.11, 0.085, 0.075), clouds);
    milky *= 0.35 + 0.65 * band;
    // A deep blue airglow line near the Earth's limb is added by the atmosphere
    // pass, so space itself stays black.
    float vignette = 0.72 + 0.28 * smoothstep(-0.4, 0.6, ray_eci.z);
    return milky * uStarAlpha * vignette + vec3(0.0015, 0.002, 0.004);
}

vec3 sun_disc(vec3 ray_eci, vec3 sun_eci) {
    float d = clamp(dot(ray_eci, sun_eci), -1.0, 1.0);
    float ang = acos(d);
    float disc = 1.0 - smoothstep(0.0065, 0.0090, ang);
    float halo = exp(-ang * 30.0) * 0.55 + exp(-ang * 7.0) * 0.075;
    vec3 core = vec3(1.0, 0.94, 0.80) * 18.0;
    vec3 glow = vec3(1.0, 0.63, 0.31) * halo * 1.8;
    return core * disc + glow;
}

vec3 moon_disc(vec3 ray_eci, vec3 moon_eci, vec3 sun_eci) {
    float d = clamp(dot(ray_eci, moon_eci), -1.0, 1.0);
    float ang = acos(d);
    const float disc_radius = 0.0185;
    if (ang > disc_radius * 1.16) return vec3(0.0);

    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), moon_eci));
    vec3 up = normalize(cross(moon_eci, right));
    vec2 p = vec2(dot(ray_eci, right), dot(ray_eci, up)) / sin(disc_radius);
    float r2 = dot(p, p);
    vec3 col = vec3(0.0);
    float halo = exp(-max(0.0, ang - disc_radius) * 55.0) * 0.10;
    col += vec3(0.55, 0.65, 0.85) * halo;

    if (r2 < 1.0) {
        vec3 n = normalize(-moon_eci * sqrt(max(0.0, 1.0 - r2)) + right * p.x + up * p.y);
        float phase = smoothstep(-0.06, 0.10, dot(n, sun_eci));
        float rim = smoothstep(0.90, 1.0, sqrt(r2));
        vec3 tex = texture(uMoonTex, p * 0.5 + 0.5).rgb;
        float alpha = texture(uMoonTex, p * 0.5 + 0.5).a;
        vec3 lit = tex * (0.045 + 1.85 * phase);
        lit += vec3(0.055, 0.070, 0.12) * (1.0 - phase);
        col = mix(col, lit, alpha * (1.0 - rim * 0.5));
    }
    return col;
}

void main() {
    vec3 ray_ecef = normalize(uForward + uRight * (vNdc.x * uTanHalfFov * uAspect) +
                              uUp * (vNdc.y * uTanHalfFov));
    vec3 ray_eci = normalize(eci_from_ecef(ray_ecef));

    vec3 space = space_color(ray_eci, true);

    // The local sky only exists above the horizon.  Below it the globe covers
    // the frame, so the gradient can safely extend past the limb for haze.
    float up_component = dot(ray_ecef, uLocalUp);
    float sun_up = dot(uLocalUp, uSunEcef);
    float day = smoothstep(-0.14, 0.22, sun_up);
    float dusk = exp(-pow(sun_up / 0.16, 2.0));
    float horizon = 0.5 + 0.5 * smoothstep(-0.25, 0.35, up_component);
    float altitude_fade = smoothstep(0.0, 0.55, horizon);

    vec3 night_zenith = vec3(0.0025, 0.0065, 0.018);
    vec3 night_horizon = vec3(0.020, 0.042, 0.075);
    vec3 day_zenith = vec3(0.055, 0.19, 0.48);
    vec3 day_horizon = vec3(0.55, 0.75, 0.92);
    vec3 zenith = mix(night_zenith, day_zenith, day);
    vec3 horizon_col = mix(night_horizon, day_horizon, day);
    vec3 sky = mix(zenith, horizon_col, pow(smoothstep(-0.1, 1.0, up_component), 0.55));

    // Warm scattering at sunrise and sunset.
    sky = mix(sky, vec3(0.85, 0.34, 0.12) * (0.25 + 0.9 * dusk),
              dusk * (1.0 - smoothstep(0.0, 0.55, up_component)) * 0.62);

    float sun_angle = acos(clamp(dot(ray_ecef, uSunEcef), -1.0, 1.0));
    sky += vec3(1.0, 0.72, 0.38) * exp(-sun_angle * 8.0) * (0.18 + 0.55 * day);
    sky += vec3(1.0, 0.88, 0.68) * exp(-sun_angle * 38.0) * 0.35 * day;

    // Stars and the Milky Way fade out under a bright sky.
    float star_visibility = uStarAlpha * (1.0 - smoothstep(0.02, 0.55, day));
    space *= star_visibility;
    sky = mix(space, sky, uSurface * altitude_fade);

    vec3 sun = sun_disc(ray_eci, uSunEci);
    vec3 moon = moon_disc(ray_eci, uMoonEci, uSunEci);
    // Sun and Moon are celestial objects, present in both space and the sky.
    sky += sun * mix(1.0, 0.82, uSurface * day);
    sky += moon * mix(1.0, 0.55 + 0.45 * (1.0 - day), uSurface);
    fragColor = vec4(sky, 1.0);
}
