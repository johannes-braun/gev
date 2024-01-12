#pragma once
#include <rnu/math/math.hpp>

namespace gev::audio
{
  class listener
  {
  public:
    static void set_volume(float v);
    static float get_volume();
    static void set_orientation(rnu::quat orientation);
    static rnu::quat get_orientation();
    static void set_position(rnu::vec3 position);
    static rnu::vec3 get_position();
  };
}