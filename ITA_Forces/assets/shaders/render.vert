#version 150 core
uniform mat4 ciModelViewProjection;

in vec4 v_Position;
in vec3 v_Velocity;
in float v_Alpha;

out float Alpha;

void main()
{
	gl_Position = ciModelViewProjection * v_Position;

	Alpha = length(v_Velocity)/length(vec3(16,9,0));
	Alpha = clamp(Alpha,0.1,1);
	gl_PointSize = 2.0;
}