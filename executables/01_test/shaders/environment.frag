#version 460 core

layout(location = 0) in vec2 texcoord;

layout(set = 0, binding = 0) uniform Camera
{
  mat4 view_matrix;
  mat4 proj_matrix;
} camera;

layout(location = 0) out vec4 color;

void main()
{
  mat4 vp = camera.proj_matrix * camera.view_matrix;
  mat4 ivp = inverse(vp);

  vec4 direction_near = ivp * vec4(texcoord, -1, 1);
  vec4 direction_far = ivp * vec4(texcoord, 1, 1);
  direction_near /= direction_near.w;
  direction_far /= direction_far.w;
  vec3 direction = normalize(direction_far.xyz - direction_near.xyz);

  
  vec3 sky = 1.6 * mix(vec3(59, 55, 53) / 255.0, vec3(99, 93, 90) / 255.0, smoothstep(-0.8, -0.1, direction.y));
  sky = mix(sky, vec3(1), smoothstep(-0.06, 0.0, direction.y));
  sky = mix(sky, 0.9*vec3(98, 154, 245) / 255, smoothstep(0.0, 0.4, direction.y));

  color = vec4(sky, 1.0);
}