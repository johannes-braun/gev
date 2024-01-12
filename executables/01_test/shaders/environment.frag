#version 460 core

layout(location = 0) in vec4 direction;

layout(location = 0) out vec4 color;

void main()
{
  vec3 norm_direction = normalize(direction.xyz);

  vec3 sky = 1.2 * mix(vec3(59, 55, 53) / 255.0, vec3(99, 93, 90) / 255.0, 
    smoothstep(-0.8, -0.1, norm_direction.y));
  sky = mix(sky, vec3(1), smoothstep(-0.06, 0.0, norm_direction.y));
  sky = mix(sky, vec3(98, 154, 245) / 255, smoothstep(0.0, 0.4, norm_direction.y));

  color = vec4(pow(sky, vec3(2.2)), 1.0);
}