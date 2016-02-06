// Update logic adapted from
// ofxGpuParticles by Neil Mendoza
// https://github.com/neilmendoza/ofxGpuParticles

#version 150 core
uniform vec3	u_ForcePos[2];
uniform float	u_Count;
uniform float	u_ForceScale;
uniform float	u_Radius;
uniform float	u_MagScale;
uniform float	u_Damping;
uniform vec2	u_Bounds;
in vec3 v_Position;
in vec3 v_Velocity;

out vec3 o_Position;
out vec3 o_Velocity;

void main()
{
	vec3 force = vec3(0);
	int f = int(u_Count);
	for(int i=0;i<f;++i)
	{
		vec3 direction = u_ForcePos[i] - v_Position;

		float radSq = (u_Bounds.x+u_Bounds.y)*u_Radius;
		float mag = length(direction);
		float distSquared = mag*mag;
		float magnitude = u_MagScale * (1.0 - distSquared / radSq);

		force += step(distSquared, radSq) * magnitude * normalize(direction);
    }
	o_Velocity = v_Velocity + force;	
    o_Velocity *= u_Damping;
    o_Position = v_Position + o_Velocity*u_ForceScale;
	
	if(o_Position.x <0 )
		o_Position.x = u_Bounds.x;
	else if(o_Position.x>u_Bounds.x)
		o_Position.x = 0;
	if(o_Position.y <0 )
		o_Position.y = u_Bounds.y;
	else if(o_Position.y>u_Bounds.y)
		o_Position.y = 0;
}