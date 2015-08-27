#version 150 core
uniform mat4 ciModelViewProjection;

uniform float u_PointSize;

in vec4 v_Position;
in float v_Alpha;

out float Alpha;

void main()
{
	gl_Position = ciModelViewProjection * v_Position;
	gl_PointSize  = u_PointSize;

	Alpha = v_Alpha;
}