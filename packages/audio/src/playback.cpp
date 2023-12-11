#include <gev/audio/playback.hpp>
#include <AL/al.h>
#include "source_allocator.hpp"

namespace gev::audio
{
  playback::playback(std::shared_ptr<source_allocator> alloc, std::shared_ptr<sound> content)
  {
    _alloc = std::move(alloc);
    _content = std::move(content);
    _source = _alloc->allocate_source();
    _buffer = _alloc->allocate_buffer();

    // Todo: stream?
    alBufferData(get_raw(_buffer.get()), _content->_channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16,
      _content->_data.data(), _content->_data.size() * sizeof(uint16_t), _content->_sample_rate);
    alSourcei(get_raw(_source.get()), AL_BUFFER, get_raw(_buffer.get()));
  }

  playback::~playback()
  {
    stop();
    set_looping(false);
    set_volume(1.0f);
    set_pitch(1.0f);
    set_position({ 0,0,0 });
    alSourcei(get_raw(_source.get()), AL_BUFFER, 0);
  }

  void playback::play() const
  {
    alSourcePlay(get_raw(_source.get()));
  }

  void playback::pause() const
  {
    alSourcePause(get_raw(_source.get()));
  }

  void playback::stop() const
  {
    alSourceStop(get_raw(_source.get()));
  }

  void playback::set_looping(bool loops) const
  {
    alSourcef(get_raw(_source.get()), AL_LOOPING, loops);
  }

  bool playback::is_looping() const
  {
    float value = 0;
    alGetSourcef(get_raw(_source.get()), AL_LOOPING, &value);
    return value;
  }

  void playback::set_volume(float v) const
  {
    alSourcef(get_raw(_source.get()), AL_GAIN, v);
  }

  float playback::get_volume() const
  {
    float value = 0;
    alGetSourcef(get_raw(_source.get()), AL_GAIN, &value);
    return value;
  }

  void playback::set_pitch(float p) const
  {
    alSourcef(get_raw(_source.get()), AL_PITCH, p);
  }

  float playback::get_pitch() const
  {
    float value = 0;
    alGetSourcef(get_raw(_source.get()), AL_PITCH, &value);
    return value;
  }

  void playback::set_position(rnu::vec3 position) const
  {
    alSourcefv(get_raw(_source.get()), AL_POSITION, position.data());
  }

  rnu::vec3 playback::get_position() const
  {
    rnu::vec3 position = 0;
    alGetSourcefv(get_raw(_source.get()), AL_POSITION, position.data());
    return position;
  }

  bool playback::is_playing() const
  {
    ALint state = 0;
    alGetSourcei(get_raw(_source.get()), AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
  }

  bool playback::is_paused() const
  {
    ALint state = 0;
    alGetSourcei(get_raw(_source.get()), AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
  }
}