#pragma once

#include <gev/scenery/component.hpp>
#include <gev/scenery/collider.hpp>
#include "skin_component.hpp"

class remote_controller_component : public gev::scenery::component
{
public:
  void own();

  void spawn() override;
  void update() override;

private:
  std::weak_ptr<gev::scenery::collider_component> _collider;
  std::weak_ptr<skin_component> _skin;

  rnu::smooth<rnu::vec3> _smooth;
  rnu::quat _target_rotation;
  bool _is_grounded = false;

  bool _is_owned = false;
};