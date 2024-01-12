#pragma once

#include <gev/buffer.hpp>
#include <gev/image.hpp>
#include <gev/game/sync_buffer.hpp>
#include <memory>
#include <rnu/math/math.hpp>
#include <unordered_map>

namespace gev::game
{
  class shadow_map_holder;

  class shadow_map_instance
  {
    friend class shadow_map_holder;

  public:
    void update_transform(rnu::mat4 const& transform);
    void destroy();

    void make_csm_root(int num_cascades);
    void set_cascade_split(float depth);

  private:
    std::weak_ptr<shadow_map_holder> _holder;
    std::shared_ptr<gev::image> _map;
    std::size_t _byte_offset = 0;
  };

  class shadow_map_holder : public std::enable_shared_from_this<shadow_map_holder>
  {
  public:
    constexpr static std::uint32_t binding_instances = 0;
    constexpr static std::uint32_t binding_maps = 1;
    constexpr static std::size_t min_reserved_elements = 32;

    constexpr static std::size_t max_num_shadow_maps = 64;

    shadow_map_holder();

    std::shared_ptr<shadow_map_instance> instantiate(std::shared_ptr<gev::image> map, rnu::mat4 matrix);
    void destroy(std::shared_ptr<gev::image> map);

    vk::DescriptorSet descriptor() const;

    void sync(vk::CommandBuffer c);

    void update_matrix_internal(std::size_t offset, rnu::mat4 matrix);
    void make_csm_root_internal(std::size_t offset, int num_cascades);
    void set_cascade_split_internal(std::size_t offset, float depth);

  private:
    void include_update_region(std::size_t begin, std::size_t end);

    struct sm_info
    {
      rnu::mat4 matrix;
      rnu::mat4 inverse_matrix;
      int map_id;
      int num_cascades; // This plus the next N-1
      float csm_split;    // depth
      int metadata2;    // TBD
    };

    struct map_info
    {
      vk::UniqueImageView view;
      std::size_t index;
    };

    vk::UniqueDescriptorPool _map_pool;
    vk::UniqueDescriptorSet _map_descriptor;

    std::unordered_map<std::shared_ptr<gev::image>, map_info> _instance_refs;
    std::vector<sm_info> _map_instances;
    std::vector<std::shared_ptr<shadow_map_instance>> _instances;
    std::size_t _update_region_start = std::numeric_limits<std::size_t>::max();
    std::size_t _update_region_end = std::numeric_limits<std::size_t>::lowest();
    std::unique_ptr<gev::game::sync_buffer> _instances_buffer;

    vk::UniqueSampler _sampler;
  };
}    // namespace gev::game