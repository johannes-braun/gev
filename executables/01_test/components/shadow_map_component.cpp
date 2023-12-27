#include "shadow_map_component.hpp"

void shadow_map_component::spawn()
{
  auto const r = gev::service<gev::game::mesh_renderer>();
  _camera = owner()->get<camera_component>();
  auto cam_ptr = _camera.lock();
  _depth_renderer = std::make_shared<gev::game::renderer>(_size, vk::SampleCountFlagBits::e1);
  _depth_renderer->set_color_format(vk::Format::eR32G32Sfloat);
  _depth_renderer->add_color_usage(vk::ImageUsageFlagBits::eStorage);
  _depth_renderer->set_clear_color({1.0f, 1.0f, 0.0f, 0.0f});

  _projection = {-10, 10, -10, 10, 0, 50};
  cam_ptr->camera()->set_projection(_projection.matrix());

  _blur_dst =
    gev::image_creator::get()
      .size(_size.width, _size.height)
      .format(vk::Format::eR32G32Sfloat)
      .type(vk::ImageType::e2D)
      .usage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc)
      .build();
  _blur_dst_view = _blur_dst->create_view(vk::ImageViewType::e2D);
  _blur1 = std::make_unique<blur>(blur_dir::vertical);
  _blur2 = std::make_unique<blur>(blur_dir::horizontal);
}

void shadow_map_component::activate()
{
  if (!_camera.expired() && !_map_instance)
  {
    auto const r = gev::service<gev::game::mesh_renderer>();
    auto cam = _camera.lock();
    auto img = _depth_renderer->color_image();
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
  auto& c = frame.command_buffer;

  auto cam_ptr = _camera.lock();
  cam_ptr->camera()->set_transform(owner()->global_transform());

  auto const r = gev::service<gev::game::mesh_renderer>();
  // SHADOW MAP
  auto const prev_camera = r->get_camera();
  r->set_camera(cam_ptr->camera());
  r->try_flush(c);
  _depth_renderer->prepare_frame(c, true, true);
  _depth_renderer->begin_render(c, true, true);
  r->render(c, 0, 0, _size.width, _size.height, gev::game::pass_id::shadow, vk::SampleCountFlagBits::e1);
  _depth_renderer->end_render(c);
  r->set_camera(prev_camera);

  auto src = _depth_renderer->color_image();
  auto src_view = _depth_renderer->color_image_view();
  _blur1->apply(c, *src, src_view, *_blur_dst, _blur_dst_view.get());
  _blur2->apply(c, *_blur_dst, _blur_dst_view.get(), *src, src_view);

  src->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead);

  if (!_camera.expired())
  {
    auto cam = _camera.lock();
    _map_instance->update_transform(cam->camera()->projection() * cam->camera()->view());
  }
}