#pragma once

#include <gev/buffer.hpp>
#include <gev/game/distance_field.hpp>
#include <memory>
#include <rnu/math/math.hpp>

namespace gev::game
{
  class distance_field_instance
  {
    friend class distance_field_holder;

  public:
    void update_transform(rnu::mat4 const& transform);

  private:
    std::weak_ptr<distance_field_holder> _holder;
    std::size_t _byte_offset = 0;
  };

  enum class field_id : std::size_t
  {
  };

  class distance_field_holder : public std::enable_shared_from_this<distance_field_holder>
  {
  public:
    constexpr static std::uint32_t binding_instances = 0;
    constexpr static std::uint32_t binding_fields = 1;
    constexpr static std::size_t min_reserved_elements = 32;

    constexpr static std::size_t max_num_distance_fields = 4096;

    distance_field_holder();

    field_id register_field(std::shared_ptr<distance_field> field);
    std::shared_ptr<distance_field_instance> instantiate(field_id field, rnu::mat4 transform);

    vk::DescriptorSet descriptor() const;
    vk::DescriptorSetLayout layout() const;

    void try_flush_buffer();

    void update_transform_internal(std::size_t offset, rnu::mat4 transform);

  private:
    void include_update_region(std::size_t begin, std::size_t end);

    struct df_info
    {
      rnu::mat4 inverse_transform;
      rnu::vec3 bmin;
      int field_id;
      rnu::vec3 bmax;
      int metadata;    // TBD
    };

    vk::UniqueDescriptorPool _field_pool;
    vk::UniqueDescriptorSetLayout _field_layout;
    vk::UniqueDescriptorSet _field_descriptor;
    std::vector<std::shared_ptr<distance_field>> _fields;

    std::vector<df_info> _field_instances;
    std::size_t _update_region_start = std::numeric_limits<std::size_t>::max();
    std::size_t _update_region_end = std::numeric_limits<std::size_t>::lowest();
    std::unique_ptr<gev::buffer> _instances_buffer;
  };
}    // namespace gev::game