R"(
#version 330 core
#extension GL_ARB_separate_shader_objects : require

layout (location = 0) out vec4 o_Color;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform float u_Time;
uniform float u_Scale;
uniform float u_Opacity;

#define PI 3.1415926538

// Function to calculate the polar coordinate in a specific point
vec2 polar(vec2 UV, vec2 center, float radialScale, float lengthScale) {
    vec2 delta = UV - center;
    float radius = length(delta) * 2.0 * radialScale;

    float angleRad = atan(delta.y, delta.x);
    float angle = (angleRad + PI) / (2.0 * PI) * lengthScale;

    return vec2(radius, angle);
}

void main() {
    // Slow down the time
    float time = u_Time;

    // Center of the UV
    vec2 center = vec2(0.5, 0.5);

    // Mirror the UV (this change the whirlpool spin direction)
    vec2 coord = v_TexCoord;
    coord.y = 1.0 - coord.y;

    // Get polar coordinate of this point
    vec2 polarColor = polar(coord, center, 1., 1.);

	float animR = (polarColor.r * 1.5) + time;
	float animG = (polarColor.r * 1.5) + polarColor.g;

	vec2 animUV = vec2(animR, animG);
	animUV = vec2(fract(animR), fract(animG));

	// Use the modified polar coordinates to sample a noise texture
	vec4 animSwirl = texture(u_Texture, animUV);

	// Use the step function to get only the white shape of the whirling "foam"
	float animStep = step(u_Scale, animSwirl.r);
	vec4 animSwirlStep = vec4(animStep);

	// Create a fade circle image, darker on the inside and lighter on the outside.
	float fadeValue = pow((polarColor.r / 1.0), 2.0);
	vec4 fadeCircle = vec4(fadeValue, fadeValue, fadeValue, 1.0);

	// Invert both the swirl step and the fade circle
	vec4 animSwirlStepInv = vec4(1.0 - animSwirlStep.rgb, 1.0);
	vec4 fadeCircleInv = vec4(1.0 - fadeCircle.rgb, 1.0);

	// Multiply the inverted swirl with the fading circle
	vec4 fadedSwirl = vec4(animSwirlStepInv.rgb, 1.0) * fadeCircleInv;

	// Discard any non opaque pixel
	if (fadedSwirl.r < 0.01)
		discard;

	// Get a fading value where the center is more transparent
	float fadedSwirlInvValue = 1.0 - ((fadedSwirl.r + fadedSwirl.g + fadedSwirl.b) / 3.0);

	// Add an external opacity value
	float finalValue = fadedSwirlInvValue * u_Opacity;

	// Keep the final value above 0 transparency
	finalValue = max(finalValue, 0.01);

    //o_Color = vec4(polarColor.r, polarColor.g, 0.0, 1.0);
	//o_Color = animSwirl;
	//o_Color = animSwirlStep;
	//o_Color = fadedSwirl;
	o_Color = vec4(1.0, 1.0, 1.0, finalValue) * v_Color;
}
)"