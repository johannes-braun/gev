#include <gev/game/camera.hpp>

namespace gev::game
{
  camera::camera()
  {
    _per_frame.set_generator([&](int i) {
      per_frame_info result;
      result.descriptor = gev::engine::get().get_descriptor_allocator().allocate(_layout);
      result.uniform_buffer = std::make_unique<gev::buffer>(
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        vk::BufferCreateInfo()
        .setQueueFamilyIndices(gev::engine::get().queues().graphics_family)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setSize(sizeof(_view_matrix) + sizeof(_proj_matrix))
        .setUsage(vk::BufferUsageFlagBits::eUniformBuffer));
      gev::update_descriptor(result.descriptor, 0, vk::DescriptorBufferInfo()
        .setBuffer(result.uniform_buffer->get_buffer())
        .setOffset(0)
        .setRange(result.uniform_buffer->size()), vk::DescriptorType::eUniformBuffer);
      return result;
      });
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
    projection[1][1] *= -1;
    _proj_matrix = projection;
  }

  rnu::mat4 camera::view()
  {
    return _view_matrix;
  }

  rnu::mat4 camera::projection()
  {
    return _proj_matrix;
  }

  void camera::finalize(frame const& frame, mesh_renderer const& r)
  {
    _layout = r.camera_set_layout();
    auto const& f = _per_frame[frame.frame_index];

    struct matrices
    {
      rnu::mat4 view_matrix;
      rnu::mat4 proj_matrix;
    } mats{ _view_matrix, _proj_matrix };
    f.uniform_buffer->load_data<matrices>(mats);
  }

  void camera::bind(frame const& frame, mesh_renderer const& r)
  {
    auto const& info = _per_frame[frame.frame_index];
    frame.command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, 
      r.pipeline_layout(), mesh_renderer::camera_set, info.descriptor, {});
  }

  void camera::bind(frame const& frame, vk::PipelineLayout layout) {
    auto const& info = _per_frame[frame.frame_index];
    frame.command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
      layout, mesh_renderer::camera_set, info.descriptor, {});
  }
}