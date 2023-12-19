#pragma once

#include "source_allocator.hpp"

#include <AL/al.h>
#include <AL/alc.h>
#include <gev/audio/audio.hpp>

namespace gev::audio
{
  class audio_host_impl : public audio_host
  {
  public:
    audio_host_impl();
    ~audio_host_impl();
    void activate() const override;
    std::shared_ptr<sound> load_file(std::filesystem::path const& path) const override;

  private:
    ALCdevice* _device = nullptr;
    ALCcontext* _context = nullptr;
    std::shared_ptr<source_allocator> _source_allocator;
  };
}    // namespace gev::audio