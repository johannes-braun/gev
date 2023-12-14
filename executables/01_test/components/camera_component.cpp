#include "camera_component.hpp"
#include <rnu/camera.hpp>

camera_component::camera_component()
{
  _camera = std::make_shared<gev::game::camera>();
}

void camera_component::spawn() {
  _renderer = gev::engine::get().services().resolve<gev::game::mesh_renderer>();
}

void camera_component::set_main()
{
  auto ptr = _renderer.lock();
  if(ptr)
    ptr->set_camera(_camera);
}

void camera_component::update()
{
  auto const size = gev::engine::get().swapchain_size();
  _camera->set_projection(rnu::cameraf::projection(rnu::radians(60.0f), size.width / float(size.height), 0.01f, 1000.0f));
  _camera->set_transform(owner()->global_transform());
}