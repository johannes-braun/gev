#pragma once

#include <gev/game/camera.hpp>
#include <gev/scenery/component.hpp>
#include "../main_controls.hpp"

class camera_component : public gev::scenery::component
{
public:
  camera_component();
  void set_main() const;
  void update() override;
  std::shared_ptr<gev::game::camera> const& camera() const;

  void serialize(gev::serializer& base, std::ostream& out) override;
  void deserialize(gev::serializer& base, std::istream& in) override;

private:
  std::shared_ptr<gev::game::camera> _camera;
  gev::service_proxy<main_controls> _controls;
};