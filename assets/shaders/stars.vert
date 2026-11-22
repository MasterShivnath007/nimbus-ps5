#version 330 core
layout(location = 0) in vec3 aDirection;
layout(location = 1) in float aSize;
layout(location = 2) in vec3 aColor;

uniform mat4 uViewProj;
uniform vec3 uCameraPos;
uniform float uPixelScale;
uniform float uAlpha;

out vec3 vColor;

void main() {
    vec4 clip = uViewProj * vec4(uCameraPos + aDirection * 2.0e6, 1.0);
    gl_Position = clip;
    gl_PointSize = max(1.0, aSize * uPixelScale);
    vColor = aColor * uAlpha;
}
