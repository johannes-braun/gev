#pragma once

#include <gev/engine.hpp>
#include <gev/game/mesh.hpp>
#include <memory>
#include <rnu/math/math.hpp>
#include <unordered_map>
#include <vector>

namespace gev::game
{
  class mesh_batch;
  class mesh_instance
  {
    friend class mesh_batch;

  public:
    void update_transform(rnu::mat4 const& transform);
    void destroy();

  private:
    std::shared_ptr<mesh> _mesh;
    std::weak_ptr<mesh_batch> _holder;
    std::size_t _byte_offset = 0;
  };

  enum class mesh_id : std::size_t
  {
  };

  class mesh_batch : public std::enable_shared_from_this<mesh_batch>
  {
  public:
    static constexpr std::uint32_t binding_instances = 0;
    static constexpr std::size_t min_reserved_elements = 32;

    mesh_batch();
    std::shared_ptr<mesh_instance> instantiate(std::shared_ptr<mesh> const& id, rnu::mat4 transform);
    void destroy(std::shared_ptr<mesh_instance> instance);
    void destroy(mesh_instance const& instance);
    void try_flush_buffer();

    vk::DescriptorSet descriptor() const;

    void render(vk::CommandBuffer c);

    void update_transform_internal(std::size_t offset, rnu::mat4 transform);

  private:
    void include_update_region(std::size_t begin, std::size_t end);

    struct mesh_info
    {
      rnu::mat4 transform;
      rnu::mat4 inverse_transform;
    };

    struct mesh_ref
    {
      std::size_t first_instance;
      std::size_t instance_count;
    };

    std::unordered_map<std::shared_ptr<mesh>, mesh_ref> _instance_refs;
    std::vector<std::shared_ptr<mesh_instance>> _instances;
    std::vector<mesh_info> _mesh_infos;
    std::size_t _update_region_start = std::numeric_limits<std::size_t>::max();
    std::size_t _update_region_end = std::numeric_limits<std::size_t>::lowest();
    std::unique_ptr<gev::buffer> _instances_buffer;

    vk::UniqueDescriptorPool _mesh_pool;
    vk::UniqueDescriptorSet _mesh_descriptor;
  };
}    // namespace gev::game