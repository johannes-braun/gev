#version 460 core

layout(location = 0) out vec4 direction;

layout(set = 0, binding = 0) uniform Camera
{
  mat4 view_matrix;
  mat4 proj_matrix;
  mat4 inverse_view_matrix;
  mat4 inverse_proj_matrix;
} camera;

void main()
{
  vec2 position = vec2((gl_VertexIndex & 2) >> 1, gl_VertexIndex & 1);
  vec2 texcoord = position * 4 - 1;
  gl_Position = vec4(texcoord, 0, 1);
  
  mat4 ivp = camera.inverse_view_matrix * camera.inverse_proj_matrix;
  direction = ivp * vec4(texcoord, 1, 1);
}