#pragma once

#include "camera_component.hpp"

#include <gev/scenery/component.hpp>
#include <gev/game/cascaded_shadow_mapping.hpp>
#include <rnu/camera.hpp>

class shadow_map_component : public gev::scenery::component
{
public:
  void spawn() override;
  void late_update() override;
  void activate() override;
  void deactivate() override; 

private:
  std::unique_ptr<gev::game::cascaded_shadow_mapping> _csm;
  gev::service_proxy<gev::game::mesh_renderer> _renderer;
  gev::service_proxy<main_controls> _controls;
};
