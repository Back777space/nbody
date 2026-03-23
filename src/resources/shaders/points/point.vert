#version 430 core

layout (binding = 0, std430) readonly buffer velocitiesBuffer {
    vec4 velocities[];
};

layout (binding = 1, std430) readonly buffer positionsBuffer {
    vec4 positions[];
};

layout (binding = 2, std430) buffer accBuffer {
    vec4 accelerations[];
};

layout (std140) uniform Matrices {
    mat4 projection;
    mat4 view;
    vec4 camPos;
};

out vec3 VertexCol;
out float AccMagnitude;

const float minPointSize = 1.35f;
const float pointScale = 95.0f;
const float maxVel = 18.0f;
const float maxAcc = 50.0f;

vec3 velocityToColor(vec3 vel) {
    float speed = length(vel);
    float normalized = clamp(speed / maxVel, 0.0, 1.0);

    vec3 colSlow = vec3(0.2, 0.2, 1.0);      // blue
    vec3 colMid = vec3(1.0, 0.5, 0.0);       // orange
    vec3 colFast = vec3(1.0, 1.0, 0.0);      // yellow

    vec3 color;
    if (normalized < 0.5) {
        color = mix(colSlow, colMid, normalized * 2.0);
    } else {
        color = mix(colMid, colFast, (normalized - 0.5) * 2.0);
    }

    return color;
}

float getPointSize(vec3 pointPos) {
    float size = pointScale / length(camPos.xyz - pointPos);
    return size;
}

void main() {
    vec3 pos = positions[gl_VertexID].xyz;
    vec3 vel = velocities[gl_VertexID].xyz;
    vec3 acc = accelerations[gl_VertexID].xyz;

    VertexCol = velocityToColor(vel);
    AccMagnitude = clamp(length(acc) / maxAcc, 0.0, 1.0);

    gl_Position = projection * view * vec4(pos.xyz, 1.0);
    gl_PointSize = max(getPointSize(pos), minPointSize);
}
