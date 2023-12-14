#pragma once

#include <rnu/math/math.hpp>
#include <gev/scenery/transform.hpp>
#include <gev/per_frame.hpp>
#include <gev/game/mesh_renderer.hpp>

namespace gev::game
{
  class camera
  {
  public:
    camera();

    void set_transform(rnu::mat4 transform);
    void set_view(rnu::mat4 view_matrix);
    void set_projection(rnu::mat4 projection);
    rnu::mat4 view();
    rnu::mat4 projection();

    void finalize(frame const& frame, mesh_renderer const& r);
    void bind(frame const& frame, mesh_renderer const& r);
    void bind(frame const& frame, vk::PipelineLayout layout);

  private:
    rnu::mat4 _view_matrix;
    rnu::mat4 _proj_matrix;

    struct per_frame_info
    {
      vk::DescriptorSet descriptor;
      std::unique_ptr<gev::buffer> uniform_buffer;
    };
    vk::DescriptorSetLayout _layout;
    gev::per_frame<per_frame_info> _per_frame;
  };
}