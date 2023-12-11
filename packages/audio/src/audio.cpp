#include <gev/audio/audio.hpp>

#include <AL/al.h>

#include <ogg/ogg.h>
#include <fstream>

namespace gev::audio
{
  void test(std::filesystem::path const& path)
  {
    std::ifstream file(path, std::ios::in | std::ios::binary);

  // TODO: https://indiegamedev.net/2020/01/16/how-to-stream-ogg-files-with-openal-in-c/

    ogg_sync_state ogg{};
    ogg_sync_init(&ogg);
    ogg_page page{};
    while (ogg_sync_pageout(&ogg, &page))
    {
      auto* buffer = ogg_sync_buffer(&ogg, 4096);
      file.read(buffer, 4096);
      int bytes = int(file.gcount());

      if (bytes == 0)
      {
        break;
      }

      ogg_sync_wrote(&ogg, bytes);
    }
    __debugbreak();
  }
}