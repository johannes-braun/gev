#pragma once
#pragma once

#include <gev/buffer.hpp>
#include <gev/engine.hpp>
#include <gev/per_frame.hpp>
#include <vector>
#include <rnu/math/math.hpp>
#include <gev/game/mesh.hpp>
#include <gev/game/material.hpp>
#include <gev/game/renderer.hpp>
#include <gev/game/distance_field.hpp>
#include <gev/game/distance_field_generator.hpp>

namespace gev::game
{
  class camera;
  class mesh_renderer
  {
  public:
    constexpr static std::uint32_t camera_set = 0;
    constexpr static std::uint32_t material_set = 1;
    constexpr static std::uint32_t distance_fields_set = 2;
    constexpr static std::uint32_t object_info_set = 3;
    constexpr static std::size_t min_num_instances = 64;

    mesh_renderer(vk::SampleCountFlagBits samples);
    void set_samples(vk::SampleCountFlagBits samples);

    void link_distance_field(std::shared_ptr<mesh> mesh, std::shared_ptr<distance_field> field);
    void generate_distance_field(std::shared_ptr<mesh> mesh, distance_field_generator& gen, std::uint32_t resolution);

    void set_camera(std::shared_ptr<camera> cam);
    std::shared_ptr<camera> const& get_camera() const;

    void prepare_frame(frame const& frame);
    void cleanup_frame();
    void enqueue(std::shared_ptr<mesh> mesh, std::shared_ptr<material> material, rnu::mat4 transform);
    void finalize_enqueues(frame const& frame);
    void render(renderer& r, frame const& frame);

    vk::DescriptorSetLayout camera_set_layout() const;
    vk::DescriptorSetLayout material_set_layout() const;
    vk::PipelineLayout pipeline_layout() const;

  private:
    std::unique_ptr<gev::buffer> create_buffer(std::size_t size);
    void setup_distance_field_data(frame const& frame, bool force_reallocate_descriptors = false);
    void rebuild_pipeline();

    struct instance_ref
    {
      std::shared_ptr<mesh> instance_mesh;
      std::shared_ptr<material> instance_material;
      std::size_t instance_offset;
      rnu::mat4 transform;
      rnu::mat4 inverse_transform;

      std::size_t combine_next;
    };

    struct distance_field_info
    {
      rnu::mat4 inverse_transform;
      rnu::vec3 bounds_min;
      float _padding;
      rnu::vec3 bounds_max;
      int image_index;
    };

    struct instance_info
    {
      rnu::mat4 transform;
      rnu::mat4 inverse_transform;
    };

    struct device_buffer_info
    {
      std::unique_ptr<gev::buffer> buffer;
      vk::DescriptorSet descriptor;
    };

    std::vector<std::shared_ptr<distance_field>> _distance_fields;
    std::unordered_map<mesh*, std::shared_ptr<std::size_t>> _distance_field_mapping;
    std::vector<distance_field_info> _host_distance_field_infos;
    std::vector<device_buffer_info> _distance_fields_per_frame;

    std::shared_ptr<camera> _camera;

    std::vector<instance_info> _host_instances;
    std::vector<instance_ref> _host_refs;
    std::vector<device_buffer_info> _per_frame;
    gev::buffer* _current_instance_buffer = nullptr;

    vk::UniqueDescriptorSetLayout _camera_set_layout;
    vk::UniqueDescriptorSetLayout _material_set_layout;
    vk::UniqueDescriptorSetLayout _distance_fields_set_layout;
    vk::UniqueDescriptorSetLayout _object_set_layout;
    vk::UniquePipelineLayout _mesh_pipeline_layout;
    vk::UniquePipeline _mesh_pipeline;

    descriptor_allocator _field_allocator;

    std::size_t _last_max_num_distance_field_images = 0;
    bool _pipeline_dirty = true;
    bool _distance_fields_dirty = true;
    vk::UniqueShaderModule _vertex_shader;
    vk::UniqueShaderModule _fragment_shader;
    vk::SampleCountFlagBits _samples;
  };
}