#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in vec3 a_Emission;
layout(location = 4) in uvec4 a_Joints;
layout(location = 5) in vec4 a_Weights;
uniform mat4 u_Palette[48];
uniform float u_Scale;
uniform float u_Yaw;
uniform mat4 u_ModelTransform;
uniform float u_TargetSize;
out vec3 v_Normal;
out vec4 v_Color;
out vec3 v_Emission;
void main()
{
    mat4 skin = a_Weights.x * u_Palette[a_Joints.x] + a_Weights.y * u_Palette[a_Joints.y] +
                a_Weights.z * u_Palette[a_Joints.z] + a_Weights.w * u_Palette[a_Joints.w];
    float c = cos(u_Yaw), s = sin(u_Yaw);
    mat3 turn = mat3(c, 0, -s, 0, 1, 0, s, 0, c);
    mat4 model = u_ModelTransform * skin;
    vec3 p = turn * (model * vec4(a_Position, 1)).xyz;
    vec2 pixel = vec2(0.5, 0.25) * u_TargetSize +
                 vec2(p.x + p.z, p.y - (p.x - p.z) * 0.5) * u_Scale;
    gl_Position = vec4(pixel / u_TargetSize * 2.0 - 1.0, -(p.x - p.z + p.y * 0.5) / 128.0, 1);
    v_Normal = turn * transpose(inverse(mat3(model))) * a_Normal;
    v_Color = a_Color;
    v_Emission = a_Emission;
}
