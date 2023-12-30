#pragma once
#include <gev/scenery/collider.hpp>
#include <gev/scenery/component.hpp>
#include <memory>
#include <rnu/algorithm/smooth.hpp>
#include <rnu/math/math.hpp>
#include <string>
#include "../collision_masks.hpp"

class debug_ui_component : public gev::scenery::component
{
public:
  debug_ui_component(std::string name);

  void ui();
  std::string const& name() const;

private:
  std::string _name;
};