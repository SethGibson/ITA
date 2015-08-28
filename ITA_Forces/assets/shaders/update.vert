// Update logic adapted from
// ofxGpuParticles by Neil Mendoza
// https://github.com/neilmendoza/ofxGpuParticles

#version 150 core
uniform vec3	u_ForcePos;
uniform float	u_ForceScale;
uniform float	u_Radius;
uniform float	u_MagScale;
uniform float	u_Damping;

in vec3 v_Position;
in vec3 v_Velocity;

out vec3 o_Position;
out vec3 o_Velocity;

void main()
{
	vec3 direction = u_ForcePos - v_Position;

	float radSq = 1920*1080*u_Radius;
	float mag = length(direction);
    float distSquared = mag*mag;
    float magnitude = u_MagScale * (1.0 - distSquared / radSq);

    vec3 force = step(distSquared, radSq) * magnitude * normalize(direction);
    o_Velocity = v_Velocity + force;
    
    o_Velocity *= u_Damping;
    o_Position = v_Position + o_Velocity*u_ForceScale;
	
	if(o_Position.x <0 )
		o_Position.x = 1440;
	else if(o_Position.x>1440)
		o_Position.x = 0;
	if(o_Position.y <0 )
		o_Position.y = 900;
	else if(o_Position.y>900)
		o_Position.x = 0;
}