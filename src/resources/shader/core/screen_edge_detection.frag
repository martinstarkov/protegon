R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform vec2 u_Resolution;

float kernel[9] = float[]
(
    1.0f,  1.0f, 1.0f,
    1.0f, -8.0f, 1.0f,
    1.0f,  1.0f, 1.0f
);

void main()
{
	float offset_x = 1.0f / u_Resolution.x;  
	float offset_y = 1.0f / u_Resolution.y;

	vec2 offsets[9] = vec2[]
	(
		vec2(-offset_x,  offset_y), vec2( 0.0f,    offset_y), vec2( offset_x,  offset_y),
		vec2(-offset_x,  0.0f),     vec2( 0.0f,    0.0f),     vec2( offset_x,  0.0f),
		vec2(-offset_x, -offset_y), vec2( 0.0f,   -offset_y), vec2( offset_x, -offset_y) 
	);

	vec3 color = vec3(0.0f);
    for (int i = 0; i < 9; i++) {
        color += vec3(texture(u_Texture, v_TexCoord.st + offsets[i])) * kernel[i];
	}
	o_Color = vec4(color, 1.0f) * v_Color;
}
)"