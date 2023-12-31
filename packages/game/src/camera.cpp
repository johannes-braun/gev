#include <gev/game/camera.hpp>
#include <gev/game/layouts.hpp>

namespace gev::game
{
  camera::camera()
  {
    _per_frame.descriptor =
      gev::engine::get().get_descriptor_allocator().allocate(layouts::defaults().camera_set_layout());
    _per_frame.uniform_buffer = std::make_unique<gev::game::sync_buffer>(
      sizeof(per_frame_info::matrices), vk::BufferUsageFlagBits::eUniformBuffer, 3);
    gev::update_descriptor(
      _per_frame.descriptor, 0, _per_frame.uniform_buffer->buffer(), vk::DescriptorType::eUniformBuffer);
  }

  void camera::set_transform(rnu::mat4 transform)
  {
    set_view(inverse(transform));
  }
  
  void camera::set_view(rnu::mat4 view_matrix)
  {
    _view_matrix = view_matrix;
    _per_frame.mat.view_matrix = _view_matrix;
    _per_frame.mat.inverse_view_matrix = inverse(_view_matrix);
    _per_frame.uniform_buffer->load_data<per_frame_info::matrices>(_per_frame.mat);
  }

  void camera::set_projection(game::projection projection)
  {
    _proj = projection;
    _proj_matrix = rnu::projection_matrix(_proj);
    _per_frame.mat.proj_matrix = _proj_matrix;
    _per_frame.mat.inverse_proj_matrix = inverse(_proj_matrix);
    _per_frame.uniform_buffer->load_data<per_frame_info::matrices>(_per_frame.mat);
  }

  rnu::mat4 camera::view() const
  {
    return _view_matrix;
  }

  projection camera::projection() const
  {
    return _proj;
  }

  rnu::mat4 camera::projection_matrix() const
  {
    return _proj_matrix;
  }

  void camera::sync(vk::CommandBuffer c)
  {
    _per_frame.uniform_buffer->sync(c);
  }

  vk::DescriptorSet camera::descriptor()
  {
    return _per_frame.descriptor;
  }

  void camera::bind(vk::CommandBuffer c, vk::PipelineLayout layout, std::uint32_t binding)
  {
    c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, binding, descriptor(), {});
  }
}    // namespace gev::game