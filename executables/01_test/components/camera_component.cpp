#include "camera_component.hpp"

#include <rnu/camera.hpp>

camera_component::camera_component()
{
  _camera = std::make_shared<gev::game::camera>();
}

void camera_component::set_main()
{
  auto ptr = _renderer.lock();
  if (ptr)
    ptr->set_camera(_camera);
}

std::shared_ptr<gev::game::camera> const& camera_component::camera() const
{
  return _camera;
}

void camera_component::update()
{
  _camera->set_transform(owner()->global_transform());
}