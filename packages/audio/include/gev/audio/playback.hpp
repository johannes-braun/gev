#pragma once

#include <cstdint>
#include <memory>
#include <gev/audio/audio.hpp>
#include <gev/audio/sound.hpp>
#include <rnu/math/math.hpp>

namespace gev::audio
{
  class source_allocator;
  class playback
  {
    friend class sound;

  public:
    ~playback();

    void release_source();

    void play() const;
    void pause() const;
    void stop() const;
    bool is_playing() const;
    bool is_paused() const;

    void set_looping(bool loops) const;
    bool is_looping() const;
    void set_volume(float v) const;
    float get_volume() const;
    void set_pitch(float p) const;
    float get_pitch() const;

    void set_position(rnu::vec3 position) const;
    rnu::vec3 get_position() const;

  private:
    playback(std::shared_ptr<source_allocator> alloc, std::shared_ptr<sound> content);

    std::shared_ptr<source_allocator> _alloc;
    std::shared_ptr<sound> _content;
    std::shared_ptr<audio_source_handle> _source;
    std::shared_ptr<audio_buffer_handle> _buffer;
  };
}