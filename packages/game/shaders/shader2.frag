#version 460 core

#extension GL_EXT_nonuniform_qualifier : require

const int PASS_FORWARD = 0;
const int PASS_SHADOW = 1;

layout(constant_id = 0) const int pass_id = 0;

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec3 vertex_normal;
layout(location = 2) in vec2 vertex_texcoord;
layout(location = 3) in vec3 vertex_color;

layout(set = 0, binding = 0) uniform Camera
{
  mat4 view_matrix;
  mat4 proj_matrix;
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
  int map_id;
  int metadata0;
  int metadata1;
  int metadata2;
};
layout(set = 3, binding = 0, std430) restrict readonly buffer ShadowMaps
{
  shadow_map maps[];
} sm;
layout(set = 3, binding = 1) uniform sampler2DShadow shadow_maps[];

layout(location = 0) out vec4 color;

vec3 tonemapFilmic(vec3 x) {
  vec3 X = max(vec3(0.0), x - 0.004);
  vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  return pow(result, vec3(2.2));
}

vec2 poissonDisk[64] = {
    vec2(-0.613392, 0.617481),
    vec2(0.170019, -0.040254),
    vec2(-0.299417, 0.791925),
    vec2(0.645680, 0.493210),
    vec2(-0.651784, 0.717887),
    vec2(0.421003, 0.027070),
    vec2(-0.817194, -0.271096),
    vec2(-0.705374, -0.668203),
    vec2(0.977050, -0.108615),
    vec2(0.063326, 0.142369),
    vec2(0.203528, 0.214331),
    vec2(-0.667531, 0.326090),
    vec2(-0.098422, -0.295755),
    vec2(-0.885922, 0.215369),
    vec2(0.566637, 0.605213),
    vec2(0.039766, -0.396100),
    vec2(0.751946, 0.453352),
    vec2(0.078707, -0.715323),
    vec2(-0.075838, -0.529344),
    vec2(0.724479, -0.580798),
    vec2(0.222999, -0.215125),
    vec2(-0.467574, -0.405438),
    vec2(-0.248268, -0.814753),
    vec2(0.354411, -0.887570),
    vec2(0.175817, 0.382366),
    vec2(0.487472, -0.063082),
    vec2(-0.084078, 0.898312),
    vec2(0.488876, -0.783441),
    vec2(0.470016, 0.217933),
    vec2(-0.696890, -0.549791),
    vec2(-0.149693, 0.605762),
    vec2(0.034211, 0.979980),
    vec2(0.503098, -0.308878),
    vec2(-0.016205, -0.872921),
    vec2(0.385784, -0.393902),
    vec2(-0.146886, -0.859249),
    vec2(0.643361, 0.164098),
    vec2(0.634388, -0.049471),
    vec2(-0.688894, 0.007843),
    vec2(0.464034, -0.188818),
    vec2(-0.440840, 0.137486),
    vec2(0.364483, 0.511704),
    vec2(0.034028, 0.325968),
    vec2(0.099094, -0.308023),
    vec2(0.693960, -0.366253),
    vec2(0.678884, -0.204688),
    vec2(0.001801, 0.780328),
    vec2(0.145177, -0.898984),
    vec2(0.062655, -0.611866),
    vec2(0.315226, -0.604297),
    vec2(-0.780145, 0.486251),
    vec2(-0.371868, 0.882138),
    vec2(0.200476, 0.494430),
    vec2(-0.494552, -0.711051),
    vec2(0.612476, 0.705252),
    vec2(-0.578845, -0.768792),
    vec2(-0.772454, -0.090976),
    vec2(0.504440, 0.372295),
    vec2(0.155736, 0.065157),
    vec2(0.391522, 0.849605),
    vec2(-0.620106, -0.328104),
    vec2(0.789239, -0.419965),
    vec2(-0.545396, 0.538133),
    vec2(-0.178564, -0.596057)
};

const vec2 shadow_offsets[] = {
  vec2( -0.94201624, -0.39906216 ),
  vec2( 0.94558609, -0.76890725 ),
  vec2( -0.094184101, -0.92938870 ),
  vec2( 0.34495938, 0.29387760 )
};

float rand(vec2 co){
  return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float shadow_map_sample(int index, out vec3 ld)
{
  float shad = 1.0;
  shadow_map sm0 = sm.maps[index];
  vec4 shadow_space = sm0.matrix * vec4(vertex_position, 1);

  vec4 projCoords = shadow_space;

  mat4 ivp = inverse(sm0.matrix);
  vec4 direction_near = ivp * vec4(projCoords.xy / projCoords.w, -1, 1);
  vec4 direction_far = ivp * vec4(projCoords.xy / projCoords.w, 1, 1);
  direction_near /= direction_near.w;
  direction_far /= direction_far.w;
  ld = normalize(direction_far.xyz - direction_near.xyz);

  // transform to [0,1] range
  projCoords.xy = projCoords.xy * 0.5 + 0.5;

  if(projCoords.z / projCoords.w > 1)
    shad = 1.0;
  else
  {
    float sh = 0.0;
    vec2 sc = 1.0 / vec2(textureSize(shadow_maps[sm0.map_id], 0));

    int count = 8;
    for(int n = 0; n < count; ++n)
    {
      vec2 off = poissonDisk[int(64 * rand(gl_FragCoord.xy * n))];
      vec2 prj = projCoords.xy + off * sc * 1;

      if(prj.x < 0 || prj.y < 0 || prj.x >= 1 || prj.y >= 1)
      {
        sh += 1.0;
        continue;
      }
        
      float bias = -0.5;
      float closestDepth = textureProj(shadow_maps[sm0.map_id], vec4(prj, projCoords.z - 0.0005 * projCoords.w, projCoords.w)).r;
      sh += closestDepth;
    }
    shad = sh / count;
  }

  return shad * smoothstep(0.5, 0.4, distance(projCoords.xy, vec2(0.5, 0.5)));
}

mat4 thresholdMatrix =
mat4(
  1.0 / 17.0,  9.0 / 17.0,  3.0 / 17.0, 11.0 / 17.0,
  13.0 / 17.0,  5.0 / 17.0, 15.0 / 17.0,  7.0 / 17.0,
  4.0 / 17.0, 12.0 / 17.0,  2.0 / 17.0, 10.0 / 17.0,
  16.0 / 17.0,  8.0 / 17.0, 14.0 / 17.0,  6.0 / 17.0
);


const vec3 ambient = 0.4 * vec3(0.24, 0.21, 0.35);
const vec3 light_color = vec3(1, 0.98, 0.96);

void main()
{
  vec4 diffuse_texture_color = sample_diffuse(vertex_texcoord);
  ivec2 px = ivec2(gl_FragCoord.xy);
  if(pass_id == PASS_SHADOW)
  {
    if(diffuse_texture_color.a < thresholdMatrix[(px.x) % 4][(px.y) % 4])
      discard;
    return;
  }

  vec3 normal = normalize(vertex_normal);
  vec3 cam_pos = inverse(camera.view_matrix)[3].xyz;
  if(!gl_FrontFacing)
    normal = -normal;

  float a = diffuse_texture_color.a * smoothstep(0, 1, distance(cam_pos, vertex_position));

  if(a < thresholdMatrix[(px.x) % 4][(px.y) % 4])
    discard;

  vec3 to_cam = normalize(cam_pos - vertex_position);
  float ao = 1.0;

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
    float s = shadow_map_sample(i, ld);
    ld *= -1;

    float ndotl = max(dot(normal, ld), 0.0);
    float shininess = max(2 / (roughness * roughness * roughness * roughness) - 2, 0);
    vec3 half_normal = normalize(ld + to_cam);
    float ndoth = pow(max(dot(normal, half_normal), 0), shininess);

    diffuse += ndotl * tex_color * s * ao * light_color;
    specular += ndoth * s * ao * light_color ;
  }

  color = vec4(vec3((diffuse + specular) + ambient * tex_color * ao), 1);
}