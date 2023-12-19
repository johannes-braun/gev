#include "source_allocator.hpp"

#include <AL/al.h>
#include <bit>

namespace gev::audio
{
  source_allocator::~source_allocator()
  {
    reset();
  }

  std::shared_ptr<audio_source_handle> source_allocator::allocate_source()
  {
    audio_source_handle* source = nullptr;
    if (!_unused_sources.empty())
    {
      source = _unused_sources.back();
      _unused_sources.pop_back();
    }
    else
    {
      // No source found. Create new.
      ALuint new_source = 0;
      alGenSources(1, &new_source);
      source = std::bit_cast<audio_source_handle*>(static_cast<std::uintptr_t>(new_source));
    }

    _used_sources.emplace(source);
    return std::shared_ptr<audio_source_handle>(source,
      [this](audio_source_handle* hnd)
      {
        _used_sources.erase(hnd);
        _unused_sources.push_back(hnd);
      });
  }

  std::shared_ptr<audio_buffer_handle> source_allocator::allocate_buffer()
  {
    audio_buffer_handle* buffer = nullptr;
    if (!_unused_sources.empty())
    {
      buffer = _unused_buffers.back();
      _unused_buffers.pop_back();
    }
    else
    {
      // No source found. Create new.
      ALuint new_buffer = 0;
      alGenBuffers(1, &new_buffer);
      buffer = std::bit_cast<audio_buffer_handle*>(static_cast<std::uintptr_t>(new_buffer));
    }

    _used_buffers.emplace(buffer);
    return std::shared_ptr<audio_buffer_handle>(buffer,
      [this](audio_buffer_handle* hnd)
      {
        _used_buffers.erase(hnd);
        _unused_buffers.push_back(hnd);
      });
  }

  void source_allocator::reset()
  {
    for (auto& src : _used_sources)
    {
      auto raw = get_raw(src);
      alDeleteSources(1, &raw);
    }
    for (auto& src : _unused_sources)
    {
      auto raw = get_raw(src);
      alDeleteSources(1, &raw);
    }
    for (auto& src : _used_buffers)
    {
      auto raw = get_raw(src);
      alDeleteBuffers(1, &raw);
    }
    for (auto& src : _unused_buffers)
    {
      auto raw = get_raw(src);
      alDeleteBuffers(1, &raw);
    }

    _used_sources.clear();
    _unused_sources.clear();
    _used_buffers.clear();
    _unused_buffers.clear();
  }

  std::uint32_t get_raw(audio_source_handle* h)
  {
    return std::uint32_t(std::bit_cast<std::uintptr_t>(h));
  }

  std::uint32_t get_raw(audio_buffer_handle* h)
  {
    return std::uint32_t(std::bit_cast<std::uintptr_t>(h));
  }
}    // namespace gev::audio