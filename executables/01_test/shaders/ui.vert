#version 460 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 color;

layout(location = 0) out vec2 pass_position;
layout(location = 1) out vec2 pass_uv;
layout(location = 2) out vec4 pass_color;

layout(push_constant) uniform Constants
{
  vec2 viewport_size;
  int use_texture;
  int is_sdf;
} options;

void main()
{
  gl_Position = vec4((position / options.viewport_size) * 2 - 1, 0, 1);
  pass_position = position;
  pass_uv = uv;
  pass_color = color;
}