#include "camera_component.hpp"

#include <gev/audio/listener.hpp>
#include <rnu/camera.hpp>

camera_component::camera_component()
{
  _camera = std::make_shared<gev::game::camera>();
}

void camera_component::set_main() const
{
  _controls->main_camera = _camera;
}

std::shared_ptr<gev::game::camera> const& camera_component::camera() const
{
  return _camera;
}

void camera_component::update()
{
  auto const& gt = owner()->global_transform();
  _camera->set_transform(gt);
  _camera->sync(gev::current_frame().command_buffer);

  if (_controls->main_camera == _camera)
  {
    gev::audio::listener::set_orientation(gt.rotation);
    gev::audio::listener::set_position(gt.position);
  }
}

void camera_component::serialize(gev::serializer& base, std::ostream& out)
{
  gev::scenery::component::serialize(base, out);
  gev::game::write_projection(_camera->projection(), out);
}

void camera_component::deserialize(gev::serializer& base, std::istream& in)
{
  gev::scenery::component::deserialize(base, in);
  gev::game::projection proj;
  gev::game::read_projection(proj, in);
  _camera->set_projection(proj);
}