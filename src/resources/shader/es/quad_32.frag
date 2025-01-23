R"(#version 300 es
precision highp float;

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in float v_TexIndex;

uniform sampler2D u_Texture[32];

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
    if (v_TexIndex == 16.0f) {
        texColor *= texture(u_Texture[16], v_TexCoord);
    }
    if (v_TexIndex == 17.0f) {
        texColor *= texture(u_Texture[17], v_TexCoord);
    }
    if (v_TexIndex == 18.0f) {
        texColor *= texture(u_Texture[18], v_TexCoord);
    }
    if (v_TexIndex == 19.0f) {
        texColor *= texture(u_Texture[19], v_TexCoord);
    }
    if (v_TexIndex == 20.0f) {
        texColor *= texture(u_Texture[20], v_TexCoord);
    }
    if (v_TexIndex == 21.0f) {
        texColor *= texture(u_Texture[21], v_TexCoord);
    }
    if (v_TexIndex == 22.0f) {
        texColor *= texture(u_Texture[22], v_TexCoord);
    }
    if (v_TexIndex == 23.0f) {
        texColor *= texture(u_Texture[23], v_TexCoord);
    }
    if (v_TexIndex == 24.0f) {
        texColor *= texture(u_Texture[24], v_TexCoord);
    }
    if (v_TexIndex == 25.0f) {
        texColor *= texture(u_Texture[25], v_TexCoord);
    }
    if (v_TexIndex == 26.0f) {
        texColor *= texture(u_Texture[26], v_TexCoord);
    }
    if (v_TexIndex == 27.0f) {
        texColor *= texture(u_Texture[27], v_TexCoord);
    }
    if (v_TexIndex == 28.0f) {
        texColor *= texture(u_Texture[28], v_TexCoord);
    }
    if (v_TexIndex == 29.0f) {
        texColor *= texture(u_Texture[29], v_TexCoord);
    }
    if (v_TexIndex == 30.0f) {
        texColor *= texture(u_Texture[30], v_TexCoord);
    }
    if (v_TexIndex == 31.0f) {
        texColor *= texture(u_Texture[31], v_TexCoord);
    }
    
    if (texColor.a == 0.0) {
        discard;
    }

	o_Color = texColor;
}
)"