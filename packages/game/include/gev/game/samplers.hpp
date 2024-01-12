#pragma once

#include <vulkan/vulkan.hpp>

namespace gev::game
{
  class samplers
  {
  public:
    static samplers const& defaults();
    samplers();

    vk::Sampler cubemap() const;
    vk::Sampler texture() const;

  private:
    vk::UniqueSampler _cubemap_sampler;
    vk::UniqueSampler _texture_sampler;
  };
}