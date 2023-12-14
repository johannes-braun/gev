#pragma once

#include <gev/scenery/component.hpp>
#include <gev/game/camera.hpp>

class camera_component : public gev::scenery::component
{
public:
  camera_component();
  void spawn() override;
  void set_main();
  void update() override;

private:
  std::weak_ptr<gev::game::mesh_renderer> _renderer;
  std::shared_ptr<gev::game::camera> _camera;
};