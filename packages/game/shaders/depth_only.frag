#version 460 core

layout(location = 2) in vec2 vertex_texcoord;

layout(set = 1, binding = 0) uniform sampler2D diffuse_texture;
layout(location = 0) out float color;

mat4 thresholdMatrix =
mat4(
  1.0 / 17.0,  9.0 / 17.0,  3.0 / 17.0, 11.0 / 17.0,
  13.0 / 17.0,  5.0 / 17.0, 15.0 / 17.0,  7.0 / 17.0,
  4.0 / 17.0, 12.0 / 17.0,  2.0 / 17.0, 10.0 / 17.0,
  16.0 / 17.0,  8.0 / 17.0, 14.0 / 17.0,  6.0 / 17.0
);

float linearize_depth(float d,float zNear,float zFar)
{
    return zNear * zFar / (zFar + d * (zNear - zFar));
}

void main()
{
  float depth = gl_FragCoord.z;
  color = linearize_depth(depth, 0.01, 100.0);
}