#version 330 core
in vec2 v_UV;
uniform sampler2D u_Model;
uniform float u_Opacity;
out vec4 FragColor;
void main()
{
    if (any(lessThan(v_UV, vec2(0))) || any(greaterThan(v_UV, vec2(1))))
    {
        discard;
    }
    FragColor = texture(u_Model, v_UV);
    if (FragColor.a < 0.001)
    {
        discard;
    }
    FragColor.rgb /= FragColor.a;
    FragColor.a *= u_Opacity;
}
