#pragma once

#include <filesystem>
#include <memory>
#include <gev/audio/sound.hpp>

namespace gev::audio
{
  enum class audio_source_handle {};
  enum class audio_buffer_handle {};

  class audio_host
  {
  public:
    static std::unique_ptr<audio_host> create();

    virtual ~audio_host() = default;
    virtual void activate() const = 0;
    virtual std::shared_ptr<sound> load_file(std::filesystem::path const& path) const = 0;
  };
}