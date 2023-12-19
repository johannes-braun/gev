#include "shadow_map_component.hpp"

void shadow_map_component::spawn()
{
  auto const r = gev::engine::get().services().resolve<gev::game::mesh_renderer>();
  _camera = owner()->get<camera_component>();
  auto cam_ptr = _camera.lock();
  _depth_renderer = std::make_shared<gev::game::renderer>(_size, vk::SampleCountFlagBits::e1);

  _projection = {-10, 10, -10, 10, 0, 50};
  cam_ptr->camera()->set_projection(_projection.matrix());
}

void shadow_map_component::activate()
{
  if (!_camera.expired() && !_map_instance)
  {
    auto const r = gev::engine::get().services().resolve<gev::game::mesh_renderer>();
    auto cam = _camera.lock();
    auto img = _depth_renderer->depth_image(0);
    _map_instance = r->get_shadow_map_holder()->instantiate(img, cam->camera()->projection() * cam->camera()->view());
  }
}

void shadow_map_component::deactivate()
{
  if (_map_instance)
  {
    _map_instance->destroy();
    _map_instance = nullptr;
  }
}

void shadow_map_component::late_update()
{
  gev::frame frame = gev::engine::get().current_frame();
  frame.frame_index = 0;

  auto cam_ptr = _camera.lock();
  cam_ptr->camera()->set_transform(owner()->global_transform());

  auto const r = gev::engine::get().services().resolve<gev::game::mesh_renderer>();
  // SHADOW MAP
  auto const prev_camera = r->get_camera();
  r->set_camera(cam_ptr->camera());
  r->try_flush(frame.command_buffer);
  _depth_renderer->prepare_frame(frame, true, false);
  _depth_renderer->begin_render(frame.command_buffer, true, false);
  r->render(0, 0, _size.width, _size.height, gev::game::pass_id::shadow, vk::SampleCountFlagBits::e1, frame);
  _depth_renderer->end_render(frame.command_buffer);
  r->set_camera(prev_camera);

  _depth_renderer->depth_image(frame.frame_index)
    ->layout(frame.command_buffer, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader,
      vk::AccessFlagBits2::eShaderSampledRead);

  if (!_camera.expired())
  {
    auto cam = _camera.lock();
    _map_instance->update_transform(cam->camera()->projection() * cam->camera()->view());
  }
}