#pragma once

#include <vulkan/vulkan.hpp>

namespace gev::game
{
  class layouts
  {
  public:
    static layouts const& defaults();

    layouts();

    vk::DescriptorSetLayout object_set_layout() const;
    vk::DescriptorSetLayout camera_set_layout() const;
    vk::DescriptorSetLayout material_set_layout() const;
    vk::DescriptorSetLayout shadow_map_layout() const;
    vk::DescriptorSetLayout skinning_set_layout() const;

  private:
    vk::UniqueDescriptorSetLayout _object_set_layout;
    vk::UniqueDescriptorSetLayout _camera_set_layout;
    vk::UniqueDescriptorSetLayout _material_set_layout;
    vk::UniqueDescriptorSetLayout _shadow_map_layout;
    vk::UniqueDescriptorSetLayout _skinning_set_layout;
  };
}    // namespace gev::game