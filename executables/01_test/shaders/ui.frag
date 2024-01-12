#version 460 core

layout(location = 0) in vec2 pass_position;
layout(location = 1) in vec2 pass_uv;
layout(location = 2) in vec4 pass_color;

layout(location = 0) out vec4 color;

layout(push_constant) uniform Constants
{
  vec2 viewport_size;
  int use_texture;
  int is_sdf;
} options;

layout(set = 0, binding = 0) uniform sampler2D image_texture;

float contour(in float d, in float w)
{
    return step(0.5 - w, d);
}

float dist(vec2 uv)
{
  return texture(image_texture, uv).r;
}

float samp(in vec2 uv, float w)
{
    return contour(dist(uv), w);
}

void main()
{
  if(options.is_sdf == 1)
  {
    float dist = dist(pass_uv);
    float afwidth = length(fwidth(pass_uv)) * 4;
    float alpha = contour(dist, afwidth);

    float dscale = 0.354; // half of 1/sqrt2;
    vec2 duv = dscale * (dFdx(pass_uv) + dFdy(pass_uv));
    vec4 box = vec4(pass_uv-duv, pass_uv+duv);

    float asum = samp(box.xy, afwidth)
               + samp(box.zw, afwidth)
               + samp(box.xw, afwidth)
               + samp(box.zy, afwidth);

    // weighted average, with 4 extra points having 0.5 weight each,
    // so 1 + 0.5*4 = 3 is the divisor
    alpha = (alpha + 0.5 * asum) / 3.;

    color = pass_color * alpha;
    color.a = alpha;
    return;
  }

  color = (options.use_texture == 1 ? texture(image_texture, pass_uv) : vec4(1)) * pass_color;
  color.rgb *= color.a;
}