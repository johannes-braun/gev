#version 460 core

layout(location = 2) in vec2 vertex_texcoord;

layout(set = 1, binding = 0) uniform sampler2D diffuse_texture;

mat4 thresholdMatrix =
mat4(
  1.0 / 17.0,  9.0 / 17.0,  3.0 / 17.0, 11.0 / 17.0,
  13.0 / 17.0,  5.0 / 17.0, 15.0 / 17.0,  7.0 / 17.0,
  4.0 / 17.0, 12.0 / 17.0,  2.0 / 17.0, 10.0 / 17.0,
  16.0 / 17.0,  8.0 / 17.0, 14.0 / 17.0,  6.0 / 17.0
);

void main()
{
  vec4 diffuse_texture_color = texture(diffuse_texture, vertex_texcoord);
  ivec2 px = ivec2(gl_FragCoord.xy);

  float a = diffuse_texture_color.a;
  if(a < thresholdMatrix[(px.x) % 4][(px.y) % 4])
    discard;
}