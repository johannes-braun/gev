#include <gev/scenery/transform.hpp>

namespace gev::scenery
{
  rnu::quat from_euler(rnu::vec3 euler)
  {
    float const roll = euler.x;
    float const yaw = euler.y;
    float const pitch = euler.z;

    float const cr = cos(roll * 0.5);
    float const sr = sin(roll * 0.5);
    float const cp = cos(pitch * 0.5);
    float const sp = sin(pitch * 0.5);
    float const cy = cos(yaw * 0.5);
    float const sy = sin(yaw * 0.5);

    rnu::quat q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;
    return q;
  }

  rnu::vec3 to_euler(rnu::quat q)
  {
    rnu::vec3 r;
    float const sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
    float const cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
    r.x = std::atan2(sinr_cosp, cosr_cosp);

    // pitch (y-axis rotation)
    float const sinp = std::sqrt(1 + 2 * (q.w * q.y - q.x * q.z));
    float const cosp = std::sqrt(1 - 2 * (q.w * q.y - q.x * q.z));
    r.z = 2 * std::atan2(sinp, cosp) - std::numbers::pi_v<float> / 2;

    // yaw (z-axis rotation)
    float const siny_cosp = 2 * (q.w * q.z + q.x * q.y);
    float const cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
    r.y = std::atan2(siny_cosp, cosy_cosp);
    return r;
  }

  transform interpolate(transform a, transform const& b, float t)
  {
    a.position = t * (b.position - a.position) + a.position;
    a.scale = t * (b.scale - a.scale) + a.scale;
    a.rotation = rnu::slerp(a.rotation, b.rotation, t);
    return a;
  }

  transform concat(transform a, transform const& b)
  {
    a.position += b.position;
    a.scale *= b.scale;
    a.rotation *= b.rotation;
    return a;
  }
}    // namespace gev::scenery