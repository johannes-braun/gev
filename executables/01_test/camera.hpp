#pragma once

#include <rnu/math/math.hpp>
#include <rnu/algorithm/smooth.hpp>
#include <rnu/camera.hpp>
#include <gev/buffer.hpp>
#include <gev/per_frame.hpp>

class camera
{
  struct per_frame_info
  {
    vk::DescriptorSet descriptor;
    std::unique_ptr<gev::buffer> uniform_buffer;
  };

public:
  camera();

  void update(double delta, int frame_index, bool allow_input);

  void bind(vk::CommandBuffer c, vk::PipelineBindPoint point, vk::PipelineLayout layout, int frame_index);

  vk::DescriptorSetLayout get_descriptor_set_layout() const;

private:

  rnu::mat4 _view_matrix;
  rnu::mat4 _proj_matrix;

  rnu::cameraf _camera;
  rnu::smooth<rnu::vec3> _axises = { { 0, 0, 0 } };
  rnu::smooth<rnu::vec2> _cursor = { { 0, 0 } };
  bool _down = false;

  vk::UniqueDescriptorSetLayout _set_layout;
  gev::per_frame<per_frame_info> _per_frame;

  rnu::vec2 _last_cursor_position = { 0, 0 };
  rnu::vec2 _dst_cursor_offset = { 0, 0 };
  float _mouse_sensitivity = 0.7f;
};