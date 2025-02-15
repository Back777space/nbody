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
};

out vec3 VertexCol;

vec3 BRcolormap(float t) {
    return vec3(
        mix(0.25, 0.85, t),     
        0.35 * (1.0 - t),      
        mix(0.70, 0.0, t)     
    );
}

vec3 RWcolormap(float t) {
    t = mix(0.0, 3.0, t);
    return vec3(
        t <= 1.0 ? t : 1.0,
        t <= 2.0 ? t - 1.0 : 1.0,
        t <= 3.0 ? t - 2.0 : 1.0
    );
}

void main() {
    float minLen = 1.5;   // expected minimum length
    float maxLen = 28;  // expected maximum length
    float len = length(velocities[gl_VertexID].xyz);
    vec4 pos = positions[gl_VertexID];
    float mass = pos.w;

    float normalizedLen = clamp((len - minLen) / (maxLen - minLen), 0, 1);
    VertexCol = BRcolormap(normalizedLen);

    gl_Position = projection * view * vec4(pos.xyz, 1.0);
    gl_PointSize = 1.5; 
}