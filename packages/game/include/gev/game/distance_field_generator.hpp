#pragma once

#include <gev/engine.hpp>
#include <rnu/math/math.hpp>
#include <gev/game/distance_field.hpp>
#include <gev/game/mesh.hpp>

namespace gev::game
{
  class distance_field_generator
  {
  public:
    distance_field_generator();

    std::unique_ptr<distance_field> generate(mesh const& obj, std::uint32_t res);
    void generate(distance_field& into, mesh const& obj);

  private:
    void generate(vk::CommandBuffer c, distance_field& into, vk::ImageView view, mesh const& obj);
    void pad_bounds(rnu::box3f& box);

    struct ddf_options
    {
      rnu::vec4 bounds_min;
      rnu::vec4 bounds_max;
      int is_signed = true;
    };

    vk::UniqueDescriptorSetLayout _set_layout;
    vk::UniquePipelineLayout _pipeline_layout;
    vk::UniquePipeline _pipeline;
    vk::DescriptorSet _descriptor;
    std::unique_ptr<gev::buffer> _options_buffer;
  };
}