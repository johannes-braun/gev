#include <gev/audio/audio.hpp>
#include "audio_host.hpp"

#include <AL/al.h>
#include <AL/alc.h>
#include <array>
#include <sndfile.hh>
#include <fstream>

namespace gev::audio
{
  std::unique_ptr<audio_host> audio_host::create()
  {
    return std::make_unique<audio_host_impl>();
  }

}