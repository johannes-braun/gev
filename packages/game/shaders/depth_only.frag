#version 460 core

layout(location = 2) in vec2 vertex_texcoord;

layout(set = 1, binding = 0) uniform sampler2D diffuse_texture;
layout(location = 0) out vec4 color;

mat4 thresholdMatrix =
mat4(
  1.0 / 17.0,  9.0 / 17.0,  3.0 / 17.0, 11.0 / 17.0,
  13.0 / 17.0,  5.0 / 17.0, 15.0 / 17.0,  7.0 / 17.0,
  4.0 / 17.0, 12.0 / 17.0,  2.0 / 17.0, 10.0 / 17.0,
  16.0 / 17.0,  8.0 / 17.0, 14.0 / 17.0,  6.0 / 17.0
);

void main()
{
  float depth = gl_FragCoord.z;
  float dx = dFdx(depth);
  float dy = dFdy(depth);
  float moment2 = depth * depth + 0.25 * (dx * dx + dy * dy);
  color = vec4(depth, moment2, 0, 0);
}