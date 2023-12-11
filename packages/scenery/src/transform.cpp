#include <gev/scenery/transform.hpp>

namespace gev::scenery
{
  rnu::mat4 transform::matrix() const
  {
    

    return rnu::translation(position) * rnu::rotation(rotation) * rnu::scale(scale);
  }

  void transform::set_matrix(rnu::mat4 mat) {
    const float mat3w = mat[3].w == 0 ? 1.f : mat[3].w;
    rnu::vec3 ax(mat[0]);
    rnu::vec3 ay(mat[1]);
    rnu::vec3 az(mat[2]);
    position = mat[3];
    position.x /= mat3w;
    position.y /= mat3w;
    position.z /= mat3w;

    ax = ax / rnu::vec3(scale.x = rnu::norm(ax));
    ay = ay / rnu::vec3(scale.y = rnu::norm(ay));
    az = az / rnu::vec3(scale.z = rnu::norm(az));

    const float m00 = ax.x;
    const float m11 = ay.y;
    const float m22 = az.z;
    const float m01 = ax.y;
    const float m10 = ay.x;
    const float m02 = ax.z;
    const float m20 = az.x;
    const float m12 = ay.z;
    const float m21 = az.y;
    float t = 0;
    if (m22 < 0)
    {
      if (m00 > m11)
      {
        t = 1 + m00 - m11 - m22;
        rotation = rnu::quat(m12 - m21, t, m01 + m10, m20 + m02);
      }
      else
      {
        t = 1 - m00 + m11 - m22;
        rotation = rnu::quat(m20 - m02, m01 + m10, t, m12 + m21);
      }
    }
    else
    {
      if (m00 < -m11)
      {
        t = 1 - m00 - m11 + m22;
        rotation = rnu::quat(m01 - m10, m20 + m02, m12 + m21, t);
      }
      else
      {
        t = 1 + m00 + m11 + m22;
        rotation = rnu::quat(t, m12 - m21, m20 - m02, m01 - m10);
      }
    }
    const float half_t_sqrt = 0.5f / rnu::sqrt(t);
    rotation.w *= half_t_sqrt;
    rotation.x *= half_t_sqrt;
    rotation.y *= half_t_sqrt;
    rotation.z *= half_t_sqrt;
  }

  transform interpolate(transform a, transform const& b, float t)
  {
    a.position = t * (b.position - a.position) + a.position;
    a.scale = t * (b.scale - a.scale) + a.scale;
    a.rotation = rnu::slerp(a.rotation, b.rotation, t);
    return a;
  }

  transform concat(transform a, transform const& b) {
    a.position += b.position;
    a.scale *= b.scale;
    a.rotation *= b.rotation;
    return a;
  }
}