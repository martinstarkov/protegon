uniform vec2 lightpos;
uniform vec3 lightColor;
uniform float screenHeight;
uniform float intensity;

void main()
{
	vec2 pixel=gl_FragCoord.xy;
	pixel.y=screenHeight-pixel.y;
	vec2 diff=lightpos-pixel;
	float distance=length(diff);
	float falloff = pow(distance, -0.3);
    vec4 color=vec4(lightColor.x,lightColor.y, lightColor.z, falloff*intensity / 10);
	gl_FragColor = color;
}
