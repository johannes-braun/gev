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

  void spawn() override;
  void update() override;
  void ui();
  std::string const& name() const;

private:
  std::weak_ptr<gev::scenery::collider_component> _collider;

  std::string _name;

  rnu::smooth<rnu::vec3> _smooth;
  rnu::quat _target_rotation;
  bool _is_grounded = false;
};