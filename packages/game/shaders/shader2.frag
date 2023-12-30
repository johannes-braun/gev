#version 460 core

#extension GL_EXT_nonuniform_qualifier : require

#include "dithering.glsl"

layout(constant_id = 0) const int pass_id = 0;

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec3 vertex_normal;
layout(location = 2) in vec2 vertex_texcoord;
layout(location = 3) in vec3 vertex_color;

layout(set = 0, binding = 0) uniform Camera
{
  mat4 view_matrix;
  mat4 proj_matrix;
  mat4 inverse_view_matrix;
  mat4 inverse_proj_matrix;
} camera;

const uint has_diffuse = 0x1;
const uint has_roughness = 0x2;

layout(set = 1, binding = 0) uniform sampler2D diffuse_texture;
layout(set = 1, binding = 1) uniform sampler2D roughness_texture;
layout(set = 1, binding = 2) restrict readonly buffer MaterialInfo
{
  uint diffuse;
  uint roughness_metadata;
  uint flags;
} material;

vec4 sample_diffuse(vec2 uv)
{
  if((material.flags & has_diffuse) != 0)
    return texture(diffuse_texture, uv);
  return unpackUnorm4x8(material.diffuse);
}

float sample_roughness(vec2 uv)
{
  if((material.flags & has_roughness) != 0)
    return texture(roughness_texture, uv).r;
  return unpackHalf2x16(material.roughness_metadata).x;
}

struct shadow_map
{
  mat4 matrix;
  mat4 inverse_matrix;
  int map_id;
  int num_cascades;
  float csm_split;    // depth
  int metadata2;
};
layout(set = 3, binding = 0, std430) restrict readonly buffer ShadowMaps
{
  shadow_map maps[];
} sm;
layout(set = 3, binding = 1) uniform sampler2D shadow_maps[];

layout(location = 0) out vec4 color;

float gamma = 2.2;

vec3 tonemapFilmic(vec3 x) {
  vec3 X = max(vec3(0.0), x - 0.004);
  vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  return pow(result, vec3(gamma));
}

#define tonemap

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

float linstep(float low, float high, float v)
{
  return clamp((v - low) / (high - low), 0.0, 1.0);
}

float shadow_vsm(int id, vec4 shadowCoord, int fac)
{
  float factor_s = max(smoothstep(0.999, 1.0, shadowCoord.s), smoothstep(0.001, 0.0, shadowCoord.s));
  float factor_t = max(smoothstep(0.999, 1.0, shadowCoord.t), smoothstep(0.001, 0.0, shadowCoord.s));

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0) 
	{
		vec2 moments = texture(shadow_maps[nonuniformEXT(id)], shadowCoord.st).xy;

    float p = step(shadowCoord.z, moments.x);
    float var = max(moments.y - moments.x * moments.x, 2e-5 / pow(10, fac));

    float d = shadowCoord.z - moments.x;
    float p_max = linstep(0.1, 1.0, var / (var + d*d));

    return min(max(factor_s, max(factor_t, max(p_max, p))), 1.0);
	}
	return 1.0;
}
 
float shadow_map_sample(int index, out vec3 ld, out vec2 uv, int fac)
{
  float shad = 1.0;
  shadow_map sm0 = sm.maps[index];
  vec4 shadow_space = sm0.matrix * vec4(vertex_position, 1);

  vec2 proj_coords = shadow_space.xy / shadow_space.w;
  mat4 ivp = sm0.inverse_matrix;
  vec4 direction_near = ivp * vec4(proj_coords, -1, 1);
  vec4 direction_far = ivp * vec4(proj_coords, 1, 1);
  direction_near /= direction_near.w;
  direction_far /= direction_far.w;
  ld = normalize(direction_far.xyz - direction_near.xyz);

  vec4 sp = biasMat * shadow_space;
  uv = sp.st / sp.w;
  return shadow_vsm(sm0.map_id, sp / sp.w, fac);
}

const vec3 ambient = 0.4 * vec3(0.24, 0.21, 0.35);
const vec3 light_color = 1.5 * vec3(1, 0.98, 0.96);

void main()
{
  vec4 diffuse_texture_color = sample_diffuse(vertex_texcoord);
  ivec2 px = ivec2(gl_FragCoord.xy);

  vec3 normal = normalize(vertex_normal);
  vec3 cam_pos = inverse(camera.view_matrix)[3].xyz;
  if(!gl_FrontFacing)
    normal = -normal;

  float a = diffuse_texture_color.a * smoothstep(0, 1, distance(cam_pos, vertex_position));
//  if(dithered_transparency(a, gl_FragCoord.xy))
//    discard;

  vec3 to_cam = normalize(cam_pos - vertex_position);

  vec3 tex_color = diffuse_texture_color.rgb;
  float roughness = sample_roughness(vertex_texcoord);

  vec3 diffuse = vec3(0);
  vec3 specular = vec3(0);

  float map_shadow = 1.0;
  float num_shadows = 0.0;
  for(int i=0; i<sm.maps.length(); ++i)
  {
    if(sm.maps[i].map_id < 0)
      break;

    num_shadows += 1.0;

    vec3 ld;
    float s = 1.0;
    int num_cascades = sm.maps[i].num_cascades;
    vec2 shadow_uv = vec2(0);
    if(num_cascades > 0)
    {
      vec4 view_pos = camera.view_matrix * vec4(vertex_position, 1);
      uint cascadeIndex = 0;
      float split_plane = 0.0f;
	    for(uint c = 0; c < num_cascades - 1; ++c) {
        float split = sm.maps[i + c + 1].csm_split;
		    if(view_pos.z < split) {	
			    cascadeIndex = c + 1;
          split_plane = split;
		    }
	    }

      int next_index = min(num_cascades, int(cascadeIndex)+1);

      float grad = 0.0;
      if(next_index != num_cascades)
      {
        float next_plane = sm.maps[next_index].csm_split;
        grad = smoothstep(0.9, 1.0, view_pos.z / next_plane);
      }

      s = shadow_map_sample(int(cascadeIndex) + i, ld, shadow_uv, 1 + int(cascadeIndex));

      if(grad != 0.0)
      {
        float s2 = shadow_map_sample(int(cascadeIndex + 1) + i, ld, shadow_uv, 1 + int(cascadeIndex + 1));
        s = mix(s, s2, grad);
      }

      i += num_cascades - 1;
    }
    else
    {
      s = shadow_map_sample(i, ld, shadow_uv, 1);
    }

    ld *= -1;

    float ndotl = max(dot(normal, ld), 0.0);
    float shininess = max(2 / (roughness * roughness * roughness * roughness) - 2, 0);
    vec3 half_normal = normalize(ld + to_cam);
    float ndoth = pow(max(dot(normal, half_normal), 0), shininess);

    diffuse += ndotl * tex_color * s * light_color;
    specular += ndoth * s * light_color;
  }

  color = vec4(tonemap(vec3((diffuse + specular) + ambient * tex_color)), 1);
}