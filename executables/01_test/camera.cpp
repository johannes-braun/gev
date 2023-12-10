#include "camera.hpp"
#include <gev/descriptors.hpp>
#include <gev/engine.hpp>

camera::camera() : _camera(rnu::vec3(0, 0, 5))
{
  _set_layout = gev::descriptor_layout_creator::get()
    .bind(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics)
    .build();

  _per_frame.set_generator([&](int i) {
    per_frame_info result;
    result.descriptor = gev::engine::get().get_descriptor_allocator().allocate(_set_layout.get());
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

void camera::update(double delta, int frame_index, bool allow_input)
{
  auto const window = gev::engine::get().window();
  auto const w = gev::engine::get().swapchain_size().width;
  auto const h = gev::engine::get().swapchain_size().height;

  _axises.to(rnu::vec3{
          glfwGetKey(window, GLFW_KEY_W) - glfwGetKey(window, GLFW_KEY_S),
            glfwGetKey(window, GLFW_KEY_A) - glfwGetKey(window, GLFW_KEY_D),
            glfwGetKey(window, GLFW_KEY_Q) - glfwGetKey(window, GLFW_KEY_E)
    } *(5.f + 10 * glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)));
  _axises.update(20 * delta);

  _camera.axis(
    float(delta),
    _axises.value().x,
    _axises.value().y,
    _axises.value().z);

  double mx, my;
  glfwGetCursorPos(window, &mx, &my);
  if (!_down && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) && allow_input)
  {
    _last_cursor_position = { mx, my };
    _dst_cursor_offset = { mx, my };
    _down = true;
    _cursor = rnu::smooth(rnu::vec2{ mx, my });
  }
  else if (!allow_input || (_down && !glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)))
  {
    _camera.mouse(0, 0, false);
    _down = false;
  }

  if (_down)
  {
    float dx = mx - _last_cursor_position.x;
    float dy = my - _last_cursor_position.y;
    _dst_cursor_offset.x += dx * _mouse_sensitivity;
    _dst_cursor_offset.y += dy * _mouse_sensitivity;
    glfwSetCursorPos(window, _last_cursor_position.x, _last_cursor_position.y);
    _cursor.to(_dst_cursor_offset);
  }
  _cursor.update(20 * delta);

  _camera.mouse(_cursor.value().x,
    _cursor.value().y, !_cursor.finished());

  _view_matrix = _camera.matrix();
  _proj_matrix = rnu::cameraf::projection(rnu::radians(60.0f), w / float(h), 0.01f, 1000.0f);
  _proj_matrix[1][1] *= -1;

  auto const& info = _per_frame[frame_index];

  struct matrices
  {
    rnu::mat4 view_matrix;
    rnu::mat4 proj_matrix;
  } mats{ _view_matrix, _proj_matrix };

  info.uniform_buffer->load_data<matrices>(mats);
}

void camera::bind(vk::CommandBuffer c, vk::PipelineBindPoint point, vk::PipelineLayout layout, int frame_index)
{
  auto const& info = _per_frame[frame_index];
  c.bindDescriptorSets(point, layout, 0, info.descriptor, {});
}

vk::DescriptorSetLayout camera::get_descriptor_set_layout() const
{
  return _set_layout.get();
}