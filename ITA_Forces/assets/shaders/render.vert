#version 150 core
uniform mat4 ciModelViewProjection;
uniform vec2 u_Bounds;

in vec4 v_Position;
in vec3 v_Velocity;

out float Alpha;

void main()
{
	gl_Position = ciModelViewProjection * v_Position;
	
	Alpha = length( vec2(v_Velocity)/(u_Bounds*0.001) );
	Alpha = clamp(Alpha,0.0,1.0);
	gl_PointSize = mix(4.0,1.0,Alpha);
}