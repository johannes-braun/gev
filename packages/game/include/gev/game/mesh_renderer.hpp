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
    constexpr static std::size_t min_num_instances = 64;

    mesh_renderer();

    void set_camera(std::shared_ptr<camera> cam);
    std::shared_ptr<camera> const& get_camera() const;

    void render(vk::CommandBuffer c, std::int32_t x, std::int32_t y, std::uint32_t w, std::uint32_t h, pass_id pass,
      vk::SampleCountFlagBits samples);

    std::shared_ptr<shadow_map_holder> get_shadow_map_holder() const;
    std::shared_ptr<mesh_batch> batch(std::shared_ptr<shader> const& shader, std::shared_ptr<material> const& material);

    void child_renderer(mesh_renderer& r, bool share_batches = true, bool share_shadow_maps = true);
    void try_flush_batches();
    void try_flush(vk::CommandBuffer c);

  protected:
    using batch_map = std::unordered_map<std::shared_ptr<shader>,
      std::unordered_map<std::shared_ptr<material>, std::shared_ptr<mesh_batch>>>;

    std::shared_ptr<shadow_map_holder> _shadow_map_holder;
    std::shared_ptr<batch_map> _batches;
    std::shared_ptr<camera> _camera;
  };
}    // namespace gev::game