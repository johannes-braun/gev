#version 460 core
#define TAU 6.282185317

layout(location = 0) in vec4 direction;
layout(set = 0, binding = 0) uniform samplerCube cubemap;
layout(location = 0) out vec4 color;

vec2 random_hammersley_2d(float current, float inverse_sample_count)
{
  vec2 result;
  result.x = current * inverse_sample_count;

  // Radical inverse
	uint bits = uint(current);
  bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	result.y = float(bits) * 2.3283064365386963e-10f;
  return result;
}

vec3 sample_cosine_hemisphere(vec2 uv)
{
	// (Uniformly) sample a point on the unit disk
    float r = sqrt(uv.x);
    float theta = TAU * uv.y;
    float x = r * cos(theta);
    float y = r * sin(theta);

    // Project point up to the unit sphere
    float z = float(sqrt(max(0.f, 1 - x * x - y * y)));
    return vec3(x, y, z);
}

void main()
{
  color = vec4(0);

  vec3 norm_direction = normalize(direction.xyz);
	
  vec3 u = normalize(cross(norm_direction, (abs(norm_direction.x) <= 0.6f) ? vec3(1, 0, 0) : vec3(0, 1, 0)));
  mat3 to_world = mat3(u, cross(norm_direction, u), norm_direction);

  const int count = 32;
  const int ham_samples = count;
  for(int i=0; i < count; ++i)
  {
	  const vec2 rnd	= random_hammersley_2d(float(i), 1.f/ham_samples);
	  const vec3 hemi = normalize(sample_cosine_hemisphere(rnd));
	  const vec3 ltw	= to_world * hemi;
		
	  color += textureLod(cubemap, ltw, 3);
  }

  color /= count;
}