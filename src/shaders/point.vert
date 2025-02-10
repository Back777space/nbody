#version 430 core

layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 aVel;

layout (std140) uniform Matrices
{
    mat4 projection;
    mat4 view;
};

out vec3 VertexCol;

void main()
{
    float minLen = 0.01;   // Expected minimum length
    float maxLen = 825.0;  // Expected maximum length (adjust based on actual values)

    // Compute vector length
    float len = length(aVel.xyz);

    // Normalize len to the [0,1] range
    float normalizedLen = clamp((len - minLen) / (maxLen - minLen), 0.0, 1.0);

    // Debug: Output normalized length to ensure it's between 0 and 1
    // You can use this if you have an easy way to visualize it in the shader (like setting it to a grayscale value)
    VertexCol = vec3(normalizedLen); 

    // Interpolate color based on length
    float col = mix(225, 825.0, normalizedLen);
    float c = col / 825.0;
    VertexCol = vec3(c, c, 1.0);  // Normalize to [0,1] so it's not always white

    gl_Position = projection * view * vec4(aPos.xyz, 1.0);
    gl_PointSize = 1.8; 
}