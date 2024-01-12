#pragma once

#include <gev/scenery/component.hpp>

class bone_component : public gev::scenery::component
{
public:
  bone_component() = default;
  bone_component(std::size_t index);
  std::size_t index() const;

  void serialize(gev::serializer& base, std::ostream& out) override;
  void deserialize(gev::serializer& base, std::istream& in) override;

private:
  std::size_t _index;
};