#version 330 core

layout(location = 0) in vec3 position;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;

void main()
{
    gl_Position = MVP * vec4(position,1);
}
