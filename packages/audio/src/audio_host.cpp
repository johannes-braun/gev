#include "audio_host.hpp"

namespace gev::audio
{
  audio_host_impl::audio_host_impl()
  {
    _device = alcOpenDevice(nullptr);
    _context = alcCreateContext(_device, nullptr);
    activate();
    _source_allocator = std::make_shared<source_allocator>();
  }

  audio_host_impl::~audio_host_impl()
  {
    if (_context)
      alcDestroyContext(_context);
    if (_device)
      alcCloseDevice(_device);
  }

  void audio_host_impl::activate() const
  {
    alcMakeContextCurrent(_context);
  }
  
  std::shared_ptr<sound> audio_host_impl::load_file(std::filesystem::path const& path) const
  {
    return std::shared_ptr<sound>(new sound(_source_allocator, path));
  }
}