#pragma once
#include <gev/scenery/component.hpp>
#include <gev/audio/sound.hpp>
#include <gev/audio/playback.hpp>
#include <gev/res/repo.hpp>
#include <memory>

class sound_component : public gev::scenery::component
{
public:
  void update() override;

  void pause();
  void stop();
  void continue_playing();

  void play_sound(std::shared_ptr<gev::audio::sound> sound);
  void play_sound(gev::resource_id res);

  bool is_playing() const;
  bool is_looping() const;
  float volume() const;
  float pitch() const;

  void set_looping(bool loops);
  void set_volume(float v);
  void set_pitch(float p);

  void serialize(gev::serializer& base, std::ostream& out) override;
  void deserialize(gev::serializer& base, std::istream& in) override;

private:
  std::shared_ptr<gev::audio::playback> _playback;
  bool _looping = false;
  float _volume = 1.0f;
  float _pitch = 1.0f;
};