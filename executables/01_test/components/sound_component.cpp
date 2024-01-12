#include "sound_component.hpp"

#include <gev/engine.hpp>

void sound_component::update()
{
  if (_playback)
  {
    _playback->set_position(owner()->global_transform().position);
    if (!_playback->is_playing() && !_playback->is_paused())
      _playback.reset();
  }
}

void sound_component::pause()
{
  if (_playback)
    _playback->pause();
}

void sound_component::stop()
{
  if (_playback)
    _playback.reset();
}

void sound_component::continue_playing()
{
  if (_playback)
    _playback->play();
}

void sound_component::play_sound(std::shared_ptr<gev::audio::sound> sound)
{
  _playback = sound->open_playback();
  _playback->set_looping(_looping);
  _playback->set_volume(_volume);
  _playback->set_pitch(_pitch);
  _playback->set_position(owner()->global_transform().position);
  _playback->play();
}

void sound_component::play_sound(gev::resource_id res)
{
  play_sound(gev::service<gev::audio_repo>()->get(res));
}

bool sound_component::is_playing() const
{
  return _playback && _playback->is_playing();
}

bool sound_component::is_looping() const
{
  return _looping;
}

float sound_component::volume() const
{
  return _volume;
}

float sound_component::pitch() const
{
  return _pitch;
}

void sound_component::set_looping(bool loops)
{
  _looping = loops;
  if (_playback)
    _playback->set_looping(_looping);
}

void sound_component::set_volume(float v)
{
  _volume = v;
  if (_playback)
    _playback->set_volume(_volume);
}

void sound_component::set_pitch(float p)
{
  _pitch = p;
  if (_playback)
    _playback->set_pitch(_pitch);
}

void sound_component::serialize(gev::serializer& base, std::ostream& out) {

  gev::scenery::component::serialize(base, out);

  write_typed(_looping, out);
  write_typed(_volume, out);
  write_typed(_pitch, out);
}

void sound_component::deserialize(gev::serializer& base, std::istream& in)
{
  gev::scenery::component::deserialize(base, in);

  read_typed(_looping, in);
  read_typed(_volume, in);
  read_typed(_pitch, in);
}
