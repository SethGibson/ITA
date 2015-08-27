#version 150 core

in float Alpha;

out vec4 FragColor;

void main()
{
	FragColor = vec4(1,1,1, Alpha);
}