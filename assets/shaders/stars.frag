#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() {
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(p, p);
    if (r2 > 1.0) discard;
    float alpha = 1.0 - smoothstep(0.0, 1.0, r2);
    alpha *= alpha;
    fragColor = vec4(vColor * alpha, 1.0);
}
