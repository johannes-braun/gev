#include <gev/scenery/transform.hpp>

namespace gev::scenery
{
  rnu::mat4 transform::matrix() const
  {
    return rnu::translation(position) * rnu::rotation(rotation) * rnu::scale(scale);
  }

  transform interpolate(transform a, transform const& b, float t)
  {
    a.position = t * (b.position - a.position) + a.position;
    a.scale = t * (b.scale - a.scale) + a.scale;
    a.rotation = rnu::slerp(a.rotation, b.rotation, t);
    return a;
  }
}