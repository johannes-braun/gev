#version 460 core

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;

layout(set = 0, binding = 0) uniform Camera
{
  mat4 view_matrix;
  mat4 proj_matrix;
} camera;

layout(location = 0) out vec3 vertex_position;
layout(location = 1) out vec3 vertex_normal;
layout(location = 2) out vec2 vertex_texcoord;
layout(location = 3) out vec3 vertex_color;

void main()
{
  int id = gl_VertexIndex;
  
  vertex_normal = normal;
  vertex_color = vec3(0.1, 0.6, 0.0);
  vertex_position = position.xyz;
  vertex_texcoord = texcoord;
  gl_Position = camera.proj_matrix * camera.view_matrix * vec4(vertex_position, 1);
}