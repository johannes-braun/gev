#pragma once

#include <gev/image.hpp>

enum class blur_dir : int
{
  horizontal = 0,
  vertical = 1
};

class blur
{
public:
  blur(blur_dir dir);
  void apply(
    vk::CommandBuffer c, gev::image& input, vk::ImageView input_view, gev::image& output, vk::ImageView output_view);

private:
  vk::UniquePipeline _pipeline;
  vk::UniquePipelineLayout _layout;
  vk::UniqueDescriptorSetLayout _set_layout;
  vk::UniqueSampler _sampler;
};