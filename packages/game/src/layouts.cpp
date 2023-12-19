#include <gev/descriptors.hpp>
#include <gev/engine.hpp>
#include <gev/game/layouts.hpp>
#include <gev/game/shadow_map_holder.hpp>

namespace gev::game
{
  layouts const& layouts::defaults()
  {
    static std::weak_ptr<layouts> init = gev::engine::get().services().register_service<layouts>();
    return *init.lock();
  }

  layouts::layouts()
  {
    _camera_set_layout = gev::descriptor_layout_creator::get()
                           .bind(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics)
                           .build();
    _material_set_layout =
      gev::descriptor_layout_creator::get()
        .bind(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eAllGraphics,
          vk::DescriptorBindingFlagBits::ePartiallyBound)
        .bind(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eAllGraphics,
          vk::DescriptorBindingFlagBits::ePartiallyBound)
        .bind(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics)
        .build();
    _object_set_layout = gev::descriptor_layout_creator::get()
                           .bind(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics)
                           .build();
    _shadow_map_layout =
      gev::descriptor_layout_creator::get()
        .bind(shadow_map_holder::binding_instances, vk::DescriptorType::eStorageBuffer, 1,
          vk::ShaderStageFlagBits::eAllGraphics)
        .bind(shadow_map_holder::binding_maps, vk::DescriptorType::eCombinedImageSampler,
          shadow_map_holder::max_num_shadow_maps, vk::ShaderStageFlagBits::eAllGraphics,
          vk::DescriptorBindingFlagBits::ePartiallyBound)
        .build();
    _skinning_set_layout =
      gev::descriptor_layout_creator::get()
        .bind(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics)
        .build();
  }

  vk::DescriptorSetLayout layouts::object_set_layout() const
  {
    return *_object_set_layout;
  }

  vk::DescriptorSetLayout layouts::camera_set_layout() const
  {
    return *_camera_set_layout;
  }

  vk::DescriptorSetLayout layouts::material_set_layout() const
  {
    return *_material_set_layout;
  }

  vk::DescriptorSetLayout layouts::shadow_map_layout() const
  {
    return *_shadow_map_layout;
  }

  vk::DescriptorSetLayout layouts::skinning_set_layout() const
  {
    return *_skinning_set_layout;
  }
}    // namespace gev::game