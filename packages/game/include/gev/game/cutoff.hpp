#pragma once

#include <gev/image.hpp>
#include <gev/game/render_target_2d.hpp>

namespace gev::game
{
  class cutoff
  {
  public:
    cutoff();
    void apply(vk::CommandBuffer c, float value, render_target_2d const& src, render_target_2d const& dst);

  private:
    vk::UniquePipeline _pipeline;
    vk::UniquePipelineLayout _layout;
    vk::UniqueDescriptorSetLayout _set_layout;
    vk::UniqueSampler _sampler;
  };
}    // namespace gev::game