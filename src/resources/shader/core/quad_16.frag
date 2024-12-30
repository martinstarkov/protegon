R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;
layout (location = 2) in float v_TexIndex;

uniform sampler2D u_Texture[16];

void main()
{
	vec4 texColor = v_Color;
    
    // Why? https://stackoverflow.com/a/74729081
    if (v_TexIndex == 0.0f) {
        texColor *= texture(u_Texture[0], v_TexCoord);
    }
    if (v_TexIndex == 1.0f) {
        texColor *= texture(u_Texture[1], v_TexCoord);
    }
    if (v_TexIndex == 2.0f) {
        texColor *= texture(u_Texture[2], v_TexCoord);
    }
    if (v_TexIndex == 3.0f) {
        texColor *= texture(u_Texture[3], v_TexCoord);
    }
    if (v_TexIndex == 4.0f) {
        texColor *= texture(u_Texture[4], v_TexCoord);
    }
    if (v_TexIndex == 5.0f) {
        texColor *= texture(u_Texture[5], v_TexCoord);
    }
    if (v_TexIndex == 6.0f) {
        texColor *= texture(u_Texture[6], v_TexCoord);
    }
    if (v_TexIndex == 7.0f) {
        texColor *= texture(u_Texture[7], v_TexCoord);
    }
    if (v_TexIndex == 8.0f) {
        texColor *= texture(u_Texture[8], v_TexCoord);
    }
    if (v_TexIndex == 9.0f) {
        texColor *= texture(u_Texture[9], v_TexCoord);
    }
    if (v_TexIndex == 10.0f) {
        texColor *= texture(u_Texture[10], v_TexCoord);
    }
    if (v_TexIndex == 11.0f) {
        texColor *= texture(u_Texture[11], v_TexCoord);
    }
    if (v_TexIndex == 12.0f) {
        texColor *= texture(u_Texture[12], v_TexCoord);
    }
    if (v_TexIndex == 13.0f) {
        texColor *= texture(u_Texture[13], v_TexCoord);
    }
    if (v_TexIndex == 14.0f) {
        texColor *= texture(u_Texture[14], v_TexCoord);
    }
    if (v_TexIndex == 15.0f) {
        texColor *= texture(u_Texture[15], v_TexCoord);
    }

	o_Color = texColor;
}
)"
