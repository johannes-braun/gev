#include <gev/res/serializer.hpp>

extern "C"
{
#include <zlib.h>
}

namespace gev
{
  void serializer::save_compressed(std::filesystem::path const& file, std::ostringstream& data)
  {
    auto const str = data.str();
    auto const handle = gzopen(file.string().c_str(), "w");
    gzwrite(handle, str.data(), str.size());
    gzclose(handle);
  }

  std::stringstream serializer::load_compressed(std::filesystem::path const& file)
  {
    auto const handle = gzopen(file.string().c_str(), "r");

    char buffer[4096]{};
    std::stringstream stream;
    int bytes = 0;
    while ((bytes = gzread(handle, buffer, std::size(buffer))) > 0)
    {
      stream.write(buffer, std::size_t(bytes));
    }

    gzclose(handle);
    return stream;
  }
}    // namespace gev::game