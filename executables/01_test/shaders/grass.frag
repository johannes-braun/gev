#version 460 core

layout(constant_id = 0) const int max_num_distance_fields = 256;

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec3 vertex_normal;
layout(location = 2) in vec2 vertex_texcoord;
layout(location = 3) in vec3 vertex_color;

layout(set = 0, binding = 0) uniform Camera
{
  mat4 view_matrix;
  mat4 proj_matrix;
} camera;

layout(set = 1, binding = 0) uniform sampler2D diffuse_texture;
layout(set = 1, binding = 1) uniform sampler2D roughness_texture;

struct distance_field
{
  mat4 inverse_transform;
  vec3 bounds_min;
  int field_id;
  vec3 bounds_max;
  int metadata;
};

layout(set = 2, binding = 0, std430) restrict readonly buffer DistanceFields
{
  distance_field fields[];
} ddf;
layout(set = 2, binding = 1) uniform sampler3D field_classes[max_num_distance_fields];

layout(location = 0) out vec4 color;

vec3 tonemapFilmic(vec3 x) {
  vec3 X = max(vec3(0.0), x - 0.004);
  vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  return pow(result, vec3(2.2));
}

float sd_box( vec3 p, vec3 b )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float sd_sample(vec3 in_position, const in distance_field field)
{
  in_position = (field.inverse_transform * vec4(in_position, 1)).xyz;

  vec3 raw_position = in_position;
  in_position = (in_position - field.bounds_min) / (field.bounds_max - field.bounds_min);
  float dist = texture(field_classes[field.field_id], in_position).r;

  vec3 size = field.bounds_max - field.bounds_min;
  vec3 offset = field.bounds_min + size * 0.5;
  
  float box = sd_box(raw_position - offset, size * 0.5);

  return dist + max(box,0);
}

float map(vec3 p)
{
  float tmin = 1.0 / 0.0;
  for(int i=0; i<=max_num_distance_fields; ++i)
  {
    distance_field d = ddf.fields[i];
    if(d.field_id < 0)
      return tmin;

    tmin = min(tmin, sd_sample(p, d));
  }
  return tmin;
}

float ambientOcclusion (vec3 pos, vec3 normal)
{
  float _AOStepSize  = 0.08;
  float sum    = 0;
  float maxSum = 0;
  for (int i = 0; i < 6; i ++)
  {
      vec3 p = pos + normal * (i+1) * _AOStepSize;
      sum    += 1. / pow(2., i) * map(p);
      maxSum += 1. / pow(2., i) * (i+1) * _AOStepSize;
  }
  return 0.5 + 0.5 * clamp(sum / maxSum, 0, 1);
}

float softshadow( in vec3 ro, in vec3 rd, float mint, float maxt, float k )
{
    float res = 1.0;
    float t = mint;
    for( int i=0; i<64 && t<maxt; i++ )
    {
        float h = map(ro + rd*t);
        if( h<0.001 )
            return 0.0;
        res = min( res, k*h/t );
        t += h;
    }
    return res;
}

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
//  ivec2 px = ivec2(gl_FragCoord.xy);

//  vec3 normal = normalize(vertex_normal);
//  vec3 cam_pos = inverse(camera.view_matrix)[3].xyz;
//
//  float a = diffuse_texture_color.a * smoothstep(0, 1, distance(cam_pos, vertex_position));
//
//  if(a < thresholdMatrix[(px.x) % 4][(px.y) % 4])
//    discard;
//
//  vec3 tex_color = diffuse_texture_color.rgb;
//  float roughness = texture(roughness_texture, vertex_texcoord).r;
//  
//  vec3 to_cam = normalize(cam_pos - vertex_position);
//  normal = faceforward(normal, -to_cam, normal);
//
  vec3 light = vec3(1, 2, 3) * 4;
//
  vec3 position_to_light_path = light - vertex_position;
  vec3 to_light = normalize(position_to_light_path);
//  float ndotl = max(dot(normal, to_light), 0.0);
//
//  float d = map(vertex_position);
   float shadow = softshadow(vertex_position, to_light, 0.1, 15.0, 16);
//  
//  float ao = max(ambientOcclusion(vertex_position, normal), 0.0);
//
//  float shininess = max(2 / (roughness * roughness * roughness * roughness) - 2, 0);
//
//  vec3 half_normal = normalize(to_light + to_cam);
//  float ndoth = pow(max(dot(normal, half_normal), 0), shininess);
//
  vec3 ambient = vec3(0.24, 0.21, 0.35);
//
//  vec3 light_color = vec3(1, 0.98, 0.96) ;
//
//  float ld2 = dot(position_to_light_path, position_to_light_path);
//
//  vec3 rgb = (vec3(ndotl)) * tex_color * shadow * ao * light_color + 
//    vec3(ndoth) * shadow * light_color * ao + 
//    ambient * tex_color * ao;

  color = vec4(tonemapFilmic(shadow * diffuse_texture_color.xyz + ambient * diffuse_texture_color.xyz), 1);
}