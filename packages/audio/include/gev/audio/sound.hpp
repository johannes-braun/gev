#pragma once

#include <memory>
#include <filesystem>

namespace gev::audio
{
  class source_allocator;
  class playback;

  class sound : public std::enable_shared_from_this<sound>
  {
    friend class playback;
    friend class audio_host;
    friend class audio_host_impl;

  public:
    ~sound();
    std::shared_ptr<playback> open_playback();

  private:
    sound(std::shared_ptr<source_allocator> allocator, std::filesystem::path const& path);

    void* _handle;
    std::vector<uint16_t> _data;
    std::shared_ptr<source_allocator> _allocator;

    std::size_t _frames;
    int _sample_rate;
    int _channels;
    int _format;
    int _sections;
    int _seekable;
  };
}