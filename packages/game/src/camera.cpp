#include <gev/game/camera.hpp>
#include <gev/game/layouts.hpp>

namespace gev::game
{
  camera::camera()
  {
    /*_per_frame.set_generator(
      [&](int i)
      {*/
    // per_frame_info result;
    _per_frame.descriptor =
      gev::engine::get().get_descriptor_allocator().allocate(layouts::defaults().camera_set_layout());
    _per_frame.uniform_buffer =
      gev::buffer::host_accessible(sizeof(per_frame_info::matrices), vk::BufferUsageFlagBits::eUniformBuffer);
    gev::update_descriptor(_per_frame.descriptor, 0,
      vk::DescriptorBufferInfo()
        .setBuffer(_per_frame.uniform_buffer->get_buffer())
        .setOffset(0)
        .setRange(_per_frame.uniform_buffer->size()),
      vk::DescriptorType::eUniformBuffer);
    // return result;
    //});
  }

  void camera::set_transform(rnu::mat4 transform)
  {
    _view_matrix = inverse(transform);
  }

  void camera::set_view(rnu::mat4 view_matrix)
  {
    _view_matrix = view_matrix;
  }

  void camera::set_projection(rnu::mat4 projection)
  {
    _proj_matrix = projection;
  }

  rnu::mat4 camera::view() const
  {
    return _view_matrix;
  }

  rnu::mat4 camera::projection() const
  {
    return _proj_matrix;
  }

  vk::DescriptorSet camera::descriptor()
  {
    auto& f = _per_frame;
    f.dirty |= (_view_matrix != f.mat.view_matrix).any() || (_proj_matrix != f.mat.proj_matrix).any();

    if (f.dirty)
    {
      f.mat.view_matrix = _view_matrix;
      f.mat.proj_matrix = _proj_matrix;
      f.mat.inverse_view_matrix = inverse(_view_matrix);
      f.mat.inverse_proj_matrix = inverse(_proj_matrix);
      f.uniform_buffer->load_data<per_frame_info::matrices>(f.mat);
    }

    return f.descriptor;
  }

  void camera::bind(vk::CommandBuffer c, vk::PipelineLayout layout, std::uint32_t binding)
  {
    c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, binding, descriptor(), {});
  }
}    // namespace gev::game