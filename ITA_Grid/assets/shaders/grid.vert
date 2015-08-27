#version 150 core
uniform mat4 ciModelViewProjection;

in vec4 v_Position;
in float v_Alpha;

out float Alpha;

void main()
{
	gl_Position = ciModelViewProjection * v_Position;
	gl_PointSize  = 2.0;

	Alpha = v_Alpha;
}