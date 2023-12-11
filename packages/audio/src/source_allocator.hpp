#pragma once

#include <vector>
#include <unordered_set>
#include <memory>
#include <gev/audio/audio.hpp>

namespace gev::audio
{
  std::uint32_t get_raw(audio_source_handle* h);
  std::uint32_t get_raw(audio_buffer_handle* h);

  class source_allocator
  {
  public:
    ~source_allocator();

    std::shared_ptr<audio_source_handle> allocate_source();
    std::shared_ptr<audio_buffer_handle> allocate_buffer();

    void reset();

  private:
    std::unordered_set<audio_source_handle*> _used_sources;
    std::vector<audio_source_handle*> _unused_sources;

    std::unordered_set<audio_buffer_handle*> _used_buffers;
    std::vector<audio_buffer_handle*> _unused_buffers;
  };
}