#pragma once

#include <gev/game/camera.hpp>
#include <gev/scenery/component.hpp>

class camera_component : public gev::scenery::component
{
public:
  camera_component();
  void set_main();
  void update() override;
  std::shared_ptr<gev::game::camera> const& camera() const;

private:
  std::weak_ptr<gev::game::mesh_renderer> _renderer;
  std::shared_ptr<gev::game::camera> _camera;
};