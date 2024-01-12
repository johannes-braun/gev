#pragma once
#pragma once

#include <gev/buffer.hpp>
#include <gev/engine.hpp>
#include <gev/game/distance_field_holder.hpp>
#include <gev/game/material.hpp>
#include <gev/game/mesh.hpp>
#include <gev/game/mesh_batch.hpp>
#include <gev/game/renderer.hpp>
#include <gev/game/shader.hpp>
#include <gev/game/shadow_map_holder.hpp>
#include <gev/per_frame.hpp>
#include <rnu/math/math.hpp>
#include <vector>

namespace gev::game
{
  class camera;
  class mesh_renderer
  {
  public:
    constexpr static std::uint32_t camera_set = 0;
    constexpr static std::uint32_t material_set = 1;
    constexpr static std::uint32_t object_info_set = 2;
    constexpr static std::uint32_t shadow_maps_set = 3;
    constexpr static std::uint32_t environment_set = 4;
    constexpr static std::uint32_t skin_set = 5;

    mesh_renderer();

    void render(vk::CommandBuffer c, camera const& cam, std::int32_t x, std::int32_t y, std::uint32_t w,
      std::uint32_t h, pass_id pass, vk::SampleCountFlagBits samples);

    std::shared_ptr<mesh_batch> batch(std::shared_ptr<shader> const& shader, std::shared_ptr<material> const& material);

    void sync(vk::CommandBuffer c);

    void set_environment_map(vk::DescriptorSet set);
    void set_shadow_maps(vk::DescriptorSet set);

  protected:
    using batch_map = std::unordered_map<std::shared_ptr<shader>,
      std::unordered_map<std::shared_ptr<material>, std::shared_ptr<mesh_batch>>>;

    std::shared_ptr<batch_map> _batches;

    vk::DescriptorSet _shadow_map_set;
    vk::DescriptorSet _environment_set;
  };
}    // namespace gev::game