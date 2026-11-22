#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;

uniform sampler2D uDayTex;
uniform sampler2D uNightTex;
uniform sampler2D uOceanTex;
uniform sampler2D uCloudTex;
uniform vec3 uSunDir;
uniform vec3 uCamPos;
uniform float uTime;
uniform float uClouds;
uniform float uNightBoost;
uniform float uOceanGloss;
uniform float uCityGlow;
uniform float uBaseFlat;
uniform vec3 uUnderlay;

out vec4 fragColor;

vec3 srgb_to_linear(vec3 c) {
    return pow(max(c, vec3(0.0)), vec3(2.2));
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorld);
    vec3 L = normalize(uSunDir);
    float ndl = dot(N, L);

    vec3 day = srgb_to_linear(texture(uDayTex, vUV).rgb);
    vec3 night = srgb_to_linear(texture(uNightTex, vUV).rgb);
    float ocean = texture(uOceanTex, vUV).r;
    float cloud_drift = uTime * 0.00012;
    vec2 cloud_uv = vec2(fract(vUV.x + cloud_drift), vUV.y);
    float cloud = texture(uCloudTex, cloud_uv).r;
    // A shadow slightly displaced from the cloud gives the globe depth.
    float cloud_shadow = texture(uCloudTex, cloud_uv + vec2(-0.0025, 0.0015)).r;

    float day_mix = smoothstep(-0.12, 0.10, ndl);
    float twilight = exp(-pow(ndl / 0.115, 2.0));
    float diffuse = max(ndl, 0.0);

    vec3 lit = day * (0.055 + 1.04 * diffuse);
    lit *= 1.0 - cloud_shadow * 0.20 * day_mix;
    // Subtle warm scattering at the terminator.
    lit += vec3(0.42, 0.16, 0.045) * twilight * 0.34 * (0.25 + 0.75 * day_mix);

    // Ocean sun glint.
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), mix(65.0, 135.0, ocean)) * ocean;
    lit += vec3(1.0, 0.91, 0.74) * spec * uOceanGloss * smoothstep(0.0, 0.22, ndl);

    // City lights, strongest on the dark side.
    float night_side = 1.0 - smoothstep(-0.18, 0.09, ndl);
    vec3 cities = night * (1.0 + 1.4 * uCityGlow);
    lit += cities * night_side * uNightBoost;
    lit += vec3(0.004, 0.009, 0.020) * night_side;

    // Clouds.
    vec3 cloud_color = vec3(0.93, 0.96, 1.0) * (0.06 + 1.06 * diffuse);
    cloud_color += vec3(1.0, 0.55, 0.28) * twilight * 0.34;
    lit = mix(lit, cloud_color, cloud * uClouds * smoothstep(0.02, 0.9, 1.0 - 0.4 * night_side));

    // Atmospheric rim.
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    float sun_rim = smoothstep(-0.15, 0.55, ndl);
    lit += vec3(0.13, 0.42, 0.95) * fresnel * (0.22 + 0.85 * sun_rim) * 0.72;
    lit += vec3(0.22, 0.52, 1.0) * fresnel * 0.12;
    // When the camera is close to the surface the streamed tile layer owns the
    // ground.  A flat slate underlay prevents the low-resolution globe from
    // showing through tile gaps as a giant blurred texel.
    lit = mix(lit, uUnderlay * (0.78 + 0.22 * diffuse), uBaseFlat);
    fragColor = vec4(lit, 1.0);
}
