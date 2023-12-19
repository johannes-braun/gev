#pragma once

#include <rnu/math/math.hpp>
#include <rnu/math/transform.hpp>

namespace gev::scenery
{
  using transform = rnu::transform<float>;

  rnu::quat from_euler(rnu::vec3 euler);
  rnu::vec3 to_euler(rnu::quat q);

  transform interpolate(transform a, transform const& b, float t);
  transform concat(transform a, transform const& b);
}    // namespace gev::scenery