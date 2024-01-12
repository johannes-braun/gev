#include <AL/al.h>
#include <gev/audio/listener.hpp>
#include <rnu/math/transform.hpp>

namespace gev::audio
{
  void listener::set_volume(float v)
  {
    alListenerf(AL_GAIN, v);
  }

  float listener::get_volume()
  {
    float v = 0.0f;
    alGetListenerf(AL_GAIN, &v);
    return v;
  }

  void listener::set_orientation(rnu::quat orientation)
  {
    rnu::transform<float> tf;
    tf.rotation = orientation;

    rnu::vec3 vectors[2];
    vectors[0] = tf.forward();
    vectors[1] = tf.up();
    alListenerfv(AL_ORIENTATION, vectors[0].data());
  }

  rnu::quat listener::get_orientation() {
    rnu::vec3 vectors[2];
    alGetListenerfv(AL_ORIENTATION, vectors[0].data());
    return rnu::look_at(rnu::vec3(0), vectors[0], vectors[1]);
  }

  void listener::set_position(rnu::vec3 position)
  {
    alListenerfv(AL_POSITION, position.data());
  }

  rnu::vec3 listener::get_position()
  {
    rnu::vec3 v;
    alGetListenerfv(AL_POSITION, v.data());
    return v;
  }
}    // namespace gev::audio