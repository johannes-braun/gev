#include "bone_component.hpp"

bone_component::bone_component(std::size_t index) : _index(index) {}

std::size_t bone_component::index() const
{
  return _index;
}

void bone_component::serialize(gev::serializer& base, std::ostream& out)
{
  gev::scenery::component::serialize(base, out);
  write_size(_index, out);
}

void bone_component::deserialize(gev::serializer& base, std::istream& in)
{
  gev::scenery::component::deserialize(base, in);
  read_size(_index, in);
}
