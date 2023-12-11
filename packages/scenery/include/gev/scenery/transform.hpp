#pragma once

#include <rnu/math/math.hpp>

namespace gev::scenery
{
  class transform
  {
  public:
    rnu::vec3 position = {};
    rnu::quat rotation = { 1, 0, 0, 0 };
    rnu::vec3 scale = { 1, 1, 1 };

    void set_matrix(rnu::mat4 mat);
    rnu::mat4 matrix() const;
  };

  transform interpolate(transform a, transform const& b, float t);

  transform concat(transform a, transform const& b);
}