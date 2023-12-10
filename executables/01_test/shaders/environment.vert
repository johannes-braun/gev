#version 460 core

layout(location = 0) out vec2 texcoord;

void main()
{
  vec2 position = vec2((gl_VertexIndex & 2) >> 1, gl_VertexIndex & 1);
  texcoord = position * 4 - 1;
  gl_Position = vec4(texcoord, 0, 1);
}