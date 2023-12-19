#pragma once

#include <gev/scenery/component.hpp>

class bone_component : public gev::scenery::component
{
public:
  bone_component(std::size_t index);
  std::size_t index() const;

private:
  std::size_t _index;
};