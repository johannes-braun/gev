#pragma once

#include "camera_component.hpp"
#include <gev/scenery/component.hpp>
#include <rnu/camera.hpp>
#include <rnu/algorithm/smooth.hpp>

class camera_controller : public gev::scenery::component
{
public:
  void spawn() override;
  void update() override;

private:
  std::weak_ptr<camera_component> _camera;
  rnu::cameraf _base_camera;
  rnu::smooth<rnu::vec3> _axises = { { 0, 0, 0 } };
  rnu::smooth<rnu::vec2> _cursor = { { 0, 0 } };
  bool _down = false;

  rnu::vec2 _last_cursor_position = { 0, 0 };
  rnu::vec2 _dst_cursor_offset = { 0, 0 };
  float _mouse_sensitivity = 0.7f;
};
