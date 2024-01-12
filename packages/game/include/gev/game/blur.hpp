#pragma once

#include <gev/image.hpp>
#include <gev/game/render_target_2d.hpp>

namespace gev::game
{
  enum class blur_dir : int
  {
    horizontal = 0,
    vertical = 1
  };

  class blur
  {
  public:
    blur();
    void apply(vk::CommandBuffer c, blur_dir dir, float step_size, 
      render_target_2d const& src, render_target_2d const& dst);

  private:
    vk::UniquePipeline _pipeline;
    vk::UniquePipelineLayout _layout;
    vk::UniqueDescriptorSetLayout _set_layout;
    vk::UniqueSampler _sampler;
  };
}    // namespace gev::game