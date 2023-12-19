#include "bone_component.hpp"

bone_component::bone_component(std::size_t index) : _index(index) {}

std::size_t bone_component::index() const
{
  return _index;
}