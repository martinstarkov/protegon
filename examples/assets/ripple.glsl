#option auto_layout

#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform float u_Time;

void main() {
    float time = u_Time;

    vec2 center = vec2(0.5f, 0.5f);

	float speed = 0.035f;

    vec2 uv = v_TexCoord;
		
	vec3 col = vec4(uv, 0.5f + 0.5f * sin(time), 1.0f).xyz;
   
    vec3 texcol;
			
	float x = center.x - uv.x;
	float y = center.y - uv.y;
		
	//float r = -sqrt(x * x + y * y); // Uncoment this line for symmetric ripples
	float r = -(x * x + y * y);
	float z = 1.0f + 0.5f * sin((r + time * speed) / 0.013f);
	
	texcol.x = z;
	texcol.y = z;
	texcol.z = z;
	
	o_Color = vec4(col * texcol, 1.0f) * v_Color;
}