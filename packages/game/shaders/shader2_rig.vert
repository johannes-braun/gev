#version 460 core

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;

layout(location = 3) in uvec4 joint_indices;
layout(location = 4) in vec4 joint_weights;

layout(set = 0, binding = 0) uniform Camera
{
  mat4 view_matrix;
  mat4 proj_matrix;
  mat4 inverse_view_matrix;
  mat4 inverse_proj_matrix;
} camera;

struct entity_info
{
  mat4 transform;
  mat4 inverse_transform;
};

layout(std430, set = 2, binding = 0) restrict readonly buffer EntityInfos
{
  entity_info entity_infos[];
};

layout(std430, set = 5, binding = 0) restrict readonly buffer Joints
{
  mat4 joints[];
};

layout(location = 0) out vec3 vertex_position;
layout(location = 1) out vec3 vertex_normal;
layout(location = 2) out vec2 vertex_texcoord;
layout(location = 3) out vec3 vertex_color;

void main()
{
  int id = gl_VertexIndex;
  
  entity_info info = entity_infos[gl_InstanceIndex];

  mat4 transform = info.transform;
  mat4 inverse_transform = info.inverse_transform;

  int i0 = int(joint_indices.x);
  int i1 = int(joint_indices.y);
  int i2 = int(joint_indices.z);
  int i3 = int(joint_indices.w);

  mat4 skin_matrix = joint_weights.x * joints[i0] +
    joint_weights.y * joints[i1] +
    joint_weights.z * joints[i2] +
    joint_weights.w * joints[i3];
  transform = transform * skin_matrix;
  inverse_transform = inverse(skin_matrix) * inverse_transform;

  vertex_normal = (transpose(inverse_transform) * vec4(normal, 0)).xyz;
  vertex_color = vec3(0.1, 0.6, 0.0);
  vec4 pos = transform * vec4(position.xyz, 1);
  vertex_position = pos.xyz;
  vertex_texcoord = texcoord;
  gl_Position = camera.proj_matrix * camera.view_matrix * pos;
}