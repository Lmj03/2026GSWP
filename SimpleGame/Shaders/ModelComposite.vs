#version 330 core
uniform vec2 u_Foot;
uniform vec2 u_Screen;
out vec2 v_UV;
void main()
{
    v_UV = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vec2 pixel = u_Foot + vec2((v_UV.x - 0.5) * 512, (0.25 - v_UV.y) * 512);
    gl_Position = vec4(pixel.x / u_Screen.x * 2 - 1, 1 - pixel.y / u_Screen.y * 2, 0, 1);
}
