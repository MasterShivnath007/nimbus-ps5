#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uViewProj;
uniform float uRadius;

out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;

void main() {
    vec3 world = aPos * uRadius;
    vWorld = world;
    vNormal = aNormal;
    vUV = aUV;
    gl_Position = uViewProj * vec4(world, 1.0);
}
