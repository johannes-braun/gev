#pragma once

#include "camera_component.hpp"

#include <gev/scenery/component.hpp>
#include <rnu/camera.hpp>

class shadow_map_component : public gev::scenery::component
{
public:
  void spawn() override;
  void late_update() override;
  void activate() override;
  void deactivate() override;

private:
  std::weak_ptr<camera_component> _camera;
  std::shared_ptr<gev::game::renderer> _depth_renderer;
  std::shared_ptr<gev::game::shadow_map_instance> _map_instance;

  rnu::ortho_projection<float> _projection;
  vk::Extent2D _size = vk::Extent2D(2048, 2048);
};