#include <gev/audio/sound.hpp>
#include <gev/audio/playback.hpp>
#include <AL/al.h>
#include <sndfile.h>
#include <array>

namespace gev::audio
{
  sound::sound(std::shared_ptr<source_allocator> allocator, std::filesystem::path const& path)
    : _allocator(std::move(allocator))
  {
    SF_INFO info;
    _handle = sf_open(path.string().c_str(), SFM_READ, &info);
    _frames = info.frames;
    _sample_rate = info.samplerate;
    _channels = info.channels;
    _format = info.format;
    _sections = info.sections;
    _seekable = info.seekable;

    // TODO: streaming?
    std::array<int16_t, 4096> read_buf{};
    size_t read_size = 0;
    while ((read_size = sf_read_short(static_cast<SNDFILE*>(_handle), read_buf.data(), read_buf.size())) != 0)
    {
      _data.insert(_data.end(), read_buf.begin(), read_buf.begin() + read_size);
    }
  }

  sound::~sound()
  {
    sf_close(static_cast<SNDFILE*>(_handle));
  }

  std::shared_ptr<playback> sound::open_playback() {
    return std::shared_ptr<playback>(new playback(_allocator, shared_from_this()));
  }
}