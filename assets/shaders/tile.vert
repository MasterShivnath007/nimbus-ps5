#version 330 core
layout(location = 0) in vec2 aUV;

uniform mat4 uViewProj;
uniform float uRadius;
uniform float uLon0;
uniform float uLon1;
uniform float uMerc0;
uniform float uMerc1;

out vec2 vUV;
out vec3 vWorld;
out vec3 vNormal;

void main() {
    float u = aUV.x;
    float v = aUV.y;
    float lon = radians(mix(uLon0, uLon1, u));
    float merc = mix(uMerc0, uMerc1, v);
    float lat = atan(sinh(merc));
    vec3 unit = vec3(cos(lat) * cos(lon), cos(lat) * sin(lon), sin(lat));
    vWorld = unit * uRadius;
    vNormal = unit;
    vUV = vec2(u, v);
    gl_Position = uViewProj * vec4(vWorld, 1.0);
}
