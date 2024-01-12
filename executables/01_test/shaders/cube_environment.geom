#version 460 core
#extension GL_ARB_shader_viewport_layer_array: require

// near: 0.01, far: 100.0, fov: 90°, aspect: 1:1
const mat4 inverse_projection = mat4(1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -49.995, 0, 0, -1, 50.005);

// order: +x, -x, +y, -y, +z, -z
const mat4 inverse_views[6] = {
  mat4(0, 0, -1, 0, -0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1),
  mat4(0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1),
  mat4(1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, -0, 1),
  mat4(1, -0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1),
  mat4(-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, -0, 0, 0, 1),
  mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
};

layout(triangles) in;
layout(triangle_strip, max_vertices=18) out;

layout(location = 0) out vec4 direction;

void main()
{
  for(int layer = 0; layer < 6; ++layer)
  {
    gl_Layer = layer;
    mat4 ivp = inverse_views[layer] * inverse_projection;

    for(int i = 0; i < gl_in.length(); ++i)
    {
      gl_Position = gl_in[i].gl_Position;
      direction = ivp * vec4(gl_Position.xy, 1, 1);
      EmitVertex();
    }
    EndPrimitive();
  }
}