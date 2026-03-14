#option auto_layout

#type fragment

out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform float u_Time;

void main() {
    float time = u_Time;

    vec2 center = vec2(0.5, 0.5);

	float speed = 0.035;

    vec2 uv = v_TexCoord;
		
	vec3 col = vec4(uv,0.5+0.5*sin(time),1.0).xyz;
   
    vec3 texcol;
			
	float x = center.x-uv.x;
	float y = center.y-uv.y;
		
	//float r = -sqrt(x*x + y*y); //uncoment this line to symmetric ripples
	float r = -(x*x + y*y);
	float z = 1.0 + 0.5*sin((r+time*speed)/0.013);
	
	texcol.x = z;
	texcol.y = z;
	texcol.z = z;
	
	o_Color = vec4(col*texcol,1.0) * v_Color;
}