#pragma once

#include <gev/game/mesh_renderer.hpp>
#include <gev/per_frame.hpp>
#include <gev/scenery/transform.hpp>
#include <rnu/math/math.hpp>

namespace gev::game
{
  class camera
  {
  public:
    camera();

    void set_transform(rnu::mat4 transform);
    void set_view(rnu::mat4 view_matrix);
    void set_projection(rnu::mat4 projection);
    rnu::mat4 view() const;
    rnu::mat4 projection() const;

    vk::DescriptorSet descriptor();
    void bind(vk::CommandBuffer c, vk::PipelineLayout layout, std::uint32_t binding);

  private:
    rnu::mat4 _view_matrix;
    rnu::mat4 _proj_matrix;

    struct per_frame_info
    {
      struct matrices
      {
        rnu::mat4 view_matrix = {};
        rnu::mat4 proj_matrix = {};
        rnu::mat4 inverse_view_matrix = {};
        rnu::mat4 inverse_proj_matrix = {};
      } mat;

      vk::DescriptorSet descriptor;
      std::unique_ptr<gev::buffer> uniform_buffer;
      bool dirty = true;
    };
    per_frame_info _per_frame;
  };
}    // namespace gev::game