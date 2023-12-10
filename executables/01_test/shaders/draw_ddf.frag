#version 460 core

layout(location = 0) in vec2 texcoord;

layout(set = 0, binding = 0) uniform Camera
{
  mat4 view_matrix;
  mat4 proj_matrix;
} camera;

layout(set = 1, binding = 0) uniform Bounds
{
  vec4 bmin;
  vec4 bmax;
} bounds;

layout(set = 1, binding = 1) uniform sampler3D field;

layout(location = 0) out vec4 color;

#define TRACE_PRECISION 0.001
#define TRACE_STEPS 256
#define TRACE_NO_RESULT -1.f
//#define WRITE_DEPTH

bool intersects_bounds(const vec3 origin, const vec3 direction, vec3 bmin, vec3 bmax,
                     const float maxDistance, inout float outMinDistance, inout float outMaxDistance)
{
  vec3 invDirection = 1.f / direction;
  
  // intersections with box planes parallel to x, y, z axis
  vec3 t135 = (bmin - origin) * invDirection;
  vec3 t246 = (bmax - origin) * invDirection;
  
  vec3 minValues = min(t135, t246);
  vec3 maxValues = max(t135, t246);
  
  float tmin = max(max(minValues.x, minValues.y), minValues.z);
  float tmax = min(min(maxValues.x, maxValues.y), maxValues.z);
  
  outMinDistance = min(tmin, tmax);
  outMaxDistance = max(tmin, tmax);
  return tmax >= 0 && tmin <= tmax && tmin <= maxDistance;
}

float ddf_dist(vec3 in_position)
{
  in_position = (in_position - bounds.bmin.xyz) / (bounds.bmax.xyz - bounds.bmin.xyz);
  return texture(field, in_position).r;
}

float map(vec3 in_position)
{
  return ddf_dist(in_position);
}

vec3 get_normal(vec3 p)
{
  vec2 e = vec2(1, -1) * 0.5773 * TRACE_PRECISION;
  vec3 n = vec3(e.xyy * map(p + e.xyy) + e.yyx * map(p + e.yyx) +
                e.yxy * map(p + e.yxy) + e.xxx * map(p + e.xxx));
  return normalize(n);
}
 
float march(vec3 ro, vec3 rd) 
{
  float tmin = 0;
  float tmax = 0;
  if(!intersects_bounds(ro, rd, bounds.bmin.xyz, bounds.bmax.xyz, 1.0 / 0.0, tmin, tmax))
    return TRACE_NO_RESULT;
  
  float dO = tmin;
  for(int i=0;i<TRACE_STEPS;i++)
  {
    vec3 p = ro + rd * dO;
    float ds = map(p);
    dO += ds;
    if(dO > tmax)
      return TRACE_NO_RESULT;
    if(ds < TRACE_PRECISION) 
      break;
  }
  return dO;
}

void main()
{
  mat4 vp = camera.proj_matrix * camera.view_matrix;
  mat4 ivp = inverse(vp);
  mat4 iv = inverse(camera.view_matrix);
  vec3 pos = iv[3].xyz;

  vec4 direction_near = ivp * vec4(texcoord, -1, 1);
  vec4 direction_far = ivp * vec4(texcoord, 1, 1);
  direction_near /= direction_near.w;
  direction_far /= direction_far.w;
  vec3 direction = normalize(direction_far.xyz - direction_near.xyz);

  float t = march(pos, direction);
  vec3 a = vec3(t);
  if(t != TRACE_NO_RESULT)
  {
    vec4 point = vec4(pos + t * direction, 1);
    point = vp * point;
    float d = (point.z / point.w + 1.0) * 0.5;

#ifdef WRITE_DEPTH
    gl_FragDepth = d;
#endif

    color = vec4(get_normal(pos + t * direction), 1.0);
  }
  else
  {
    color = vec4(1, 0, 0, 1);
    discard;
  }
}