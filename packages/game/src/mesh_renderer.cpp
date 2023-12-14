#include <gev/game/mesh_renderer.hpp>
#include <gev/game/camera.hpp>
#include <gev/pipeline.hpp>

#include <gev_game_shaders_files.hpp>

namespace gev::game
{
  mesh_renderer::mesh_renderer(vk::SampleCountFlagBits samples)
  {
    _samples = samples;

    _material_set_layout = gev::descriptor_layout_creator::get()
      .bind(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eAllGraphics)
      .bind(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eAllGraphics)
      .build();
    _camera_set_layout = gev::descriptor_layout_creator::get()
      .bind(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics)
      .build();
    _object_set_layout = gev::descriptor_layout_creator::get()
      .bind(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics)
      .build();
  }

  void mesh_renderer::set_samples(vk::SampleCountFlagBits samples)
  {
    _samples = samples;
    _pipeline_dirty = true;
  }

  void mesh_renderer::link_distance_field(std::shared_ptr<mesh> mesh, std::shared_ptr<distance_field> field) {

    _distance_field_mapping[mesh.get()] = std::make_shared<std::size_t>(_distance_fields.size());

    _distance_fields.push_back(std::move(field));
    _distance_fields_dirty = true;
  }

  void mesh_renderer::generate_distance_field(std::shared_ptr<mesh> mesh, distance_field_generator& gen, std::uint32_t resolution) {
    link_distance_field(std::move(mesh), gen.generate(*mesh, resolution));
  }

  void mesh_renderer::setup_distance_field_data(frame const& frame, bool force_reallocate_descriptors)
  {
    auto const num = gev::engine::get().num_images();

    bool reallocate_descriptors = force_reallocate_descriptors;
    if (_distance_fields_per_frame.size() != num)
    {
      _distance_fields_per_frame.resize(num);
      _distance_fields_dirty = true;
      reallocate_descriptors = true;
    }
    bool reassign_images = _distance_fields_dirty;
    auto& ptr = _distance_fields_per_frame[frame.frame_index];
    auto const num_distance_field_instances = std::max(_host_distance_field_infos.size(), min_num_instances);
    bool reallocate_buffer = !ptr.buffer || ptr.buffer->size() < num_distance_field_instances * sizeof(distance_field_info);
    bool reassign_buffer = false;

    auto const logn = std::floor(std::log2(std::max(1.f, _distance_fields.size() / float(min_num_instances))));
    auto const max_num_distance_fields = std::pow(2, logn) * min_num_instances;

    if (_last_max_num_distance_field_images != max_num_distance_fields)
    {
      _last_max_num_distance_field_images = max_num_distance_fields;
      rebuild_pipeline();
      reallocate_descriptors = true;
    }

    if (reallocate_descriptors)
    {
      _field_allocator.reset();
      for (auto& f : _distance_fields_per_frame)
      {
        f.descriptor = _field_allocator.allocate(_distance_fields_set_layout.get());

        if (f.buffer)
        {
          gev::update_descriptor(f.descriptor, 0, vk::DescriptorBufferInfo()
            .setBuffer(f.buffer->get_buffer())
            .setOffset(0)
            .setRange(f.buffer->size()), vk::DescriptorType::eStorageBuffer);
        }
      }
      reassign_images = true;
      reassign_buffer = true;

      gev::engine::get().logger().log("Reallocating DDF descriptors to account for {} images", _last_max_num_distance_field_images);
    }

    if (reassign_images)
    {
      for (auto& f : _distance_fields_per_frame)
      {
        for (int i = 0; i < _distance_fields.size(); ++i)
        {
          gev::update_descriptor(f.descriptor, 1, vk::DescriptorImageInfo()
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setImageView(_distance_fields[i]->image_view())
            .setSampler(_distance_fields[i]->sampler()), vk::DescriptorType::eCombinedImageSampler,
            i);
        }
      }
      gev::engine::get().logger().log("Reassigning DDF images");
    }

    if (reallocate_buffer)
    {
      auto const new_size = ptr.buffer ? 2 * ptr.buffer->size() : min_num_instances * sizeof(distance_field_info);
      ptr.buffer.reset();
      ptr.buffer = create_buffer(new_size);
      reassign_buffer = true;

      gev::engine::get().logger().log("Reallocating DDF buffer to size {}", ptr.buffer->size());
    }

    if (reassign_buffer)
    {
      gev::update_descriptor(ptr.descriptor, 0, vk::DescriptorBufferInfo()
        .setBuffer(ptr.buffer->get_buffer())
        .setOffset(0)
        .setRange(ptr.buffer->size()), vk::DescriptorType::eStorageBuffer);

      gev::engine::get().logger().log("Reallocating DDF buffer");
    }

    // append stop criterium
    _host_distance_field_infos.push_back(distance_field_info{
      .image_index = -1
      });
    _distance_fields_per_frame[frame.frame_index].buffer->load_data<distance_field_info>(_host_distance_field_infos);
    _distance_fields_dirty = false;
  }

  void mesh_renderer::prepare_frame(frame const& frame)
  {
  }

  void mesh_renderer::cleanup_frame()
  {
    _host_instances.clear();
    _host_refs.clear();
    _host_distance_field_infos.clear();
  }

  void mesh_renderer::enqueue(std::shared_ptr<mesh> mesh, std::shared_ptr<material> material, rnu::mat4 transform)
  {
    auto* mesh_ptr = mesh.get();
    auto& ref = _host_refs.emplace_back(instance_ref{
      .instance_mesh = std::move(mesh),
      .instance_material = std::move(material),
      .instance_offset = _host_instances.size(),
      .transform = transform,
      .inverse_transform = inverse(transform),
      .combine_next = 1
      });

    auto const& iter = _distance_field_mapping.find(mesh_ptr);
    if (iter != _distance_field_mapping.end())
    {
      auto const mapping = *iter->second;
      auto const& field = _distance_fields[mapping];

      _host_distance_field_infos.push_back(distance_field_info{
          .inverse_transform = ref.inverse_transform,
          .bounds_min = rnu::vec4(field->bounds().lower(), 1),
          .bounds_max = rnu::vec4(field->bounds().upper(), 1),
          .image_index = std::int32_t(mapping)
        });
    }
  }

  void mesh_renderer::rebuild_pipeline()
  {
    gev::engine::get().device().waitIdle();
    vk::SpecializationMapEntry const entry(0, 0, sizeof(std::int32_t));
    std::int32_t const dfs_size = std::int32_t(std::max(1ull, _last_max_num_distance_field_images));

    _distance_fields_set_layout = gev::descriptor_layout_creator::get()
      .bind(0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment)
      .bind(1, vk::DescriptorType::eCombinedImageSampler, dfs_size, vk::ShaderStageFlagBits::eFragment, vk::DescriptorBindingFlagBits::ePartiallyBound)
      .build();
    _mesh_pipeline_layout = gev::create_pipeline_layout({
      _camera_set_layout.get(),
      _material_set_layout.get(),
      _distance_fields_set_layout.get(),
      _object_set_layout.get()
      });

    _vertex_shader = create_shader(gev::load_spv(gev_game_shaders::shaders::shader2_vert));
    _fragment_shader = create_shader(gev::load_spv(gev_game_shaders::shaders::shader2_frag));

    _mesh_pipeline = gev::simple_pipeline_builder::get(_mesh_pipeline_layout)
      .stage(vk::ShaderStageFlagBits::eVertex, _vertex_shader)
      .stage(vk::ShaderStageFlagBits::eFragment, _fragment_shader, vk::SpecializationInfo(1, &entry, sizeof(std::int32_t), &dfs_size))
      .attribute(0, 0, vk::Format::eR32G32B32Sfloat)
      .attribute(1, 1, vk::Format::eR32G32B32Sfloat)
      .attribute(2, 2, vk::Format::eR32G32Sfloat)
      .binding(0, sizeof(rnu::vec4))
      .binding(1, sizeof(rnu::vec3))
      .binding(2, sizeof(rnu::vec2))
      .multisampling(_samples)
      .color_attachment(gev::engine::get().swapchain_format().surfaceFormat.format)
      .depth_attachment(gev::engine::get().depth_format())
      .stencil_attachment(gev::engine::get().depth_format())
      .dynamic_states(vk::DynamicState::eCullMode)
      .build();
  }

  void mesh_renderer::finalize_enqueues(frame const& frame)
  {
    if (_per_frame.size() <= frame.frame_index)
      _per_frame.resize(frame.frame_index + 1);

    auto const num_instances = std::max(_host_refs.size(), min_num_instances);

    bool const force_reallocate_ddf_descriptors = _pipeline_dirty;
    if (_pipeline_dirty)
    {
      rebuild_pipeline();
      _pipeline_dirty = false;
    }

    setup_distance_field_data(frame, force_reallocate_ddf_descriptors);

    {
      auto& ptr = _per_frame[frame.frame_index];
      if (!ptr.buffer || ptr.buffer->size() < num_instances * sizeof(instance_info))
      {
        auto const new_size = ptr.buffer ? 2 * ptr.buffer->size() : min_num_instances * sizeof(instance_info);
        ptr.buffer.reset();
        ptr.buffer = create_buffer(new_size);

        if (!ptr.descriptor)
          ptr.descriptor = gev::engine::get().get_descriptor_allocator().allocate(_object_set_layout.get());

        gev::update_descriptor(ptr.descriptor, 0, vk::DescriptorBufferInfo()
          .setBuffer(ptr.buffer->get_buffer())
          .setOffset(0)
          .setRange(ptr.buffer->size()), vk::DescriptorType::eStorageBuffer);
      }
    }

    std::sort(begin(_host_refs), end(_host_refs), [&](instance_ref const& a, instance_ref const& b) {
      auto const& a_mat_ptr = a.instance_material;
      auto const& b_mat_ptr = b.instance_material;
      auto const& a_mesh_ptr = a.instance_mesh;
      auto const& b_mesh_ptr = b.instance_mesh;
      return std::tie(a_mat_ptr, a_mesh_ptr) > std::tie(b_mat_ptr, b_mesh_ptr);
      });

    _host_instances.clear();

    if (_host_refs.empty())
      return;

    auto const same_draw_call = [this](size_t i, size_t j)
      {
        instance_ref const& a = _host_refs[i];
        instance_ref const& b = _host_refs[j];
        auto const a_mat_ptr = a.instance_material.get();
        auto const b_mat_ptr = b.instance_material.get();
        auto const a_mesh_ptr = a.instance_mesh.get();
        auto const b_mesh_ptr = b.instance_mesh.get();
        return std::tie(a_mat_ptr, a_mesh_ptr) == std::tie(b_mat_ptr, b_mesh_ptr);
      };

    std::size_t current_master_ref = -1;
    for (std::size_t i = 0; i < _host_refs.size(); ++i)
    {
      if (current_master_ref == -1)
      {
        current_master_ref = i;
      }
      else if (same_draw_call(current_master_ref, i))
      {
        _host_refs[current_master_ref].combine_next++;
      }
      else
      {
        current_master_ref = i;
      }

      _host_instances.push_back(instance_info{
        .transform = _host_refs[i].transform,
        .inverse_transform = _host_refs[i].inverse_transform
        });
    }

    _per_frame[frame.frame_index].buffer->load_data<instance_info>(_host_instances);

    _camera->finalize(frame, *this);
  }

  vk::DescriptorSetLayout mesh_renderer::camera_set_layout() const
  {
    return _camera_set_layout.get();
  }

  vk::DescriptorSetLayout mesh_renderer::material_set_layout() const
  {
    return _material_set_layout.get();
  }

  vk::PipelineLayout mesh_renderer::pipeline_layout() const
  {
    return _mesh_pipeline_layout.get();
  }

  void mesh_renderer::set_camera(std::shared_ptr<camera> cam)
  {
    _camera = std::move(cam);
  }

  std::shared_ptr<camera> const& mesh_renderer::get_camera() const
  {
    return _camera;
  }

  void mesh_renderer::render(renderer& r, frame const& frame)
  {
    auto& c = frame.command_buffer;
    r.begin_render(frame, true);
    r.bind_pipeline(frame, _mesh_pipeline.get(), _mesh_pipeline_layout.get());
    if (_host_refs.empty() || !_camera)
    {
      r.end_render(frame);
      return;
    }

    auto const& f = _per_frame[frame.frame_index];

    material* mat = nullptr;

    _camera->bind(frame, *this);
    c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _mesh_pipeline_layout.get(), distance_fields_set,
      _distance_fields_per_frame[frame.frame_index].descriptor, nullptr);
    c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _mesh_pipeline_layout.get(), object_info_set, f.descriptor, nullptr);

    int draw_calls = 0;
    for (std::size_t i = 0; i < _host_refs.size();)
    {
      auto& ref = _host_refs[i];

      if (ref.instance_material.get() != mat)
      {
        mat = ref.instance_material.get();
        mat->bind(frame, *this);
      }

      draw_calls++;
      ref.instance_mesh->draw(c, ref.combine_next, i);

      i += ref.combine_next;
    }
    r.end_render(frame);
  }

  std::unique_ptr<gev::buffer> mesh_renderer::create_buffer(std::size_t size)
  {
    return std::make_unique<gev::buffer>(
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
      vk::BufferCreateInfo().setSharingMode(vk::SharingMode::eExclusive)
      .setSize(size)
      .setUsage(vk::BufferUsageFlagBits::eStorageBuffer));
  }
}