#version 430 core
out vec4 FragColor;

in vec3 VertexCol;

const float radiusSq = 0.25;

void main() {
    vec2 distToCenter = gl_PointCoord - vec2(0.5);
    if (dot(distToCenter,distToCenter) > radiusSq) {
        discard;
    }

    FragColor = vec4(VertexCol, 1.0);
}
