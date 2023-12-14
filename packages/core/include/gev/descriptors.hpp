#pragma once

#include <array>
#include <vulkan/vulkan.hpp>
#include <gev/buffer.hpp>

namespace gev
{
  void update_descriptor(vk::DescriptorSet set, std::uint32_t binding, vk::DescriptorBufferInfo info, vk::DescriptorType type, std::uint32_t array_element = 0);
  void update_descriptor(vk::DescriptorSet set, std::uint32_t binding, vk::DescriptorImageInfo info, vk::DescriptorType type, std::uint32_t array_element = 0);

  struct descriptor_pool_scale
  {
    vk::DescriptorType type;
    float scale = 1.0f;
  };

  class descriptor_layout_creator
  {
  public:
    static descriptor_layout_creator get();

    descriptor_layout_creator& bind(std::uint32_t binding, vk::DescriptorType type, std::uint32_t count, 
      vk::ShaderStageFlags stage_flags, vk::DescriptorBindingFlags flags = {},
      vk::ArrayProxy<vk::Sampler> immutable_samplers = nullptr);

    vk::UniqueDescriptorSetLayout build();

  private:
    descriptor_layout_creator() = default;
    std::vector<vk::Sampler> _samplers;
    std::vector<vk::DescriptorSetLayoutBinding> _bindings;
    std::vector<vk::DescriptorBindingFlags> _flags;
  };

  class descriptor_allocator
  {
  public:
    descriptor_allocator(int size = 300);

    std::vector<vk::DescriptorSet> allocate(vk::DescriptorSetLayout layout, std::uint32_t count);
    vk::DescriptorSet allocate(vk::DescriptorSetLayout layout);
    void reset();

  private:
    vk::DescriptorPool get_free_pool();
    void allocate_next_pool();
    vk::DescriptorPool activate_unused_pool();

    int _pool_size = 100;

    vk::DescriptorPool _current_pool;
    std::vector<vk::UniqueDescriptorPool> _pools;
    std::vector<vk::UniqueDescriptorPool> _unused;

    std::vector<descriptor_pool_scale> _scales = {
      {.type = vk::DescriptorType::eSampler, .scale = 0.1f },
      {.type = vk::DescriptorType::eCombinedImageSampler, .scale = 3.0f },
      {.type = vk::DescriptorType::eSampledImage, .scale = 1.0f },
      {.type = vk::DescriptorType::eStorageImage, .scale = 0.2f },
      {.type = vk::DescriptorType::eUniformTexelBuffer, .scale = 0.1f },
      {.type = vk::DescriptorType::eStorageTexelBuffer, .scale = 0.1f },
      {.type = vk::DescriptorType::eUniformBuffer, .scale = 3.0f },
      {.type = vk::DescriptorType::eStorageBuffer, .scale = 2.0f },
      {.type = vk::DescriptorType::eUniformBufferDynamic, .scale = 2.0f },
      {.type = vk::DescriptorType::eStorageBufferDynamic, .scale = 1.5f },
      {.type = vk::DescriptorType::eInputAttachment, .scale = 0.4f },
      {.type = vk::DescriptorType::eInlineUniformBlock, .scale = 1.0f },
    };
    std::vector<vk::DescriptorPoolSize> _sizes;
  };
}