#include <gev/game/distance_field_generator.hpp>
#include <gev/pipeline.hpp>
#include <gev_game_shaders_files.hpp>

namespace gev::game
{
  distance_field_generator::distance_field_generator()
  {
    _set_layout =
      gev::descriptor_layout_creator::get()
        .bind(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute)
        .bind(1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute)
        .bind(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute)
        .bind(3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute)
        .build();
    _pipeline_layout = gev::create_pipeline_layout(_set_layout.get());

    auto const shader = gev::create_shader(gev::load_spv(gev_game_shaders::shaders::generate_distance_field_comp));
    _pipeline = gev::build_compute_pipeline(*_pipeline_layout, *shader);

    _options_buffer = gev::buffer::host_accessible(sizeof(ddf_options), vk::BufferUsageFlagBits::eUniformBuffer);

    _descriptor = gev::engine::get().get_descriptor_allocator().allocate(_set_layout.get());
    gev::update_descriptor(_descriptor, 0,
      vk::DescriptorBufferInfo(_options_buffer->get_buffer(), 0, _options_buffer->size()),
      vk::DescriptorType::eUniformBuffer);
  }

  std::unique_ptr<distance_field> distance_field_generator::generate(mesh const& obj, std::uint32_t res)
  {
    auto bounds = obj.bounds();
    pad_bounds(bounds);

    float max_dim = std::max(bounds.size.x, std::max(bounds.size.y, bounds.size.z));
    float step_size = max_dim / res;

    std::uint32_t sx = std::max(4, (int)std::ceil(bounds.size.x / step_size));
    std::uint32_t sy = std::max(4, (int)std::ceil(bounds.size.y / step_size));
    std::uint32_t sz = std::max(4, (int)std::ceil(bounds.size.z / step_size));

    auto result = std::make_unique<gev::image>(
      vk::ImageCreateInfo()
        .setFormat(vk::Format::eR16Sfloat)
        .setArrayLayers(1)
        .setExtent({sx, sy, sz})
        .setImageType(vk::ImageType::e3D)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setMipLevels(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled));

    std::unique_ptr<distance_field> field = std::make_unique<distance_field>(std::move(result), bounds);

    generate(*field, obj);

    return field;
  }

  void distance_field_generator::generate(distance_field& into, mesh const& obj)
  {
    auto const result_view = into.image()->create_view(vk::ImageViewType::e3D);

    gev::update_descriptor(_descriptor, 1,
      vk::DescriptorImageInfo().setImageView(result_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
      vk::DescriptorType::eStorageImage);
    gev::update_descriptor(_descriptor, 2,
      vk::DescriptorBufferInfo()
        .setBuffer(obj.vertex_buffer().get_buffer())
        .setOffset(0)
        .setRange(obj.vertex_buffer().size()),
      vk::DescriptorType::eStorageBuffer);
    gev::update_descriptor(_descriptor, 3,
      vk::DescriptorBufferInfo()
        .setBuffer(obj.index_buffer().get_buffer())
        .setOffset(0)
        .setRange(obj.index_buffer().size()),
      vk::DescriptorType::eStorageBuffer);

    gev::engine::get().execute_once([&](auto c) { generate(c, into, result_view.get(), obj); },
      gev::engine::get().queues().compute, gev::engine::get().queues().compute_command_pool.get(), true);
  }

  void distance_field_generator::generate(
    vk::CommandBuffer c, distance_field& into, vk::ImageView view, mesh const& obj)
  {
    auto bounds = obj.bounds();
    pad_bounds(bounds);

    _options_buffer->load_data<ddf_options>(ddf_options{
      .bounds_min = rnu::vec4(bounds.lower(), 1), .bounds_max = rnu::vec4(bounds.upper(), 1), .is_signed = true});

    into.image()->layout(c, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader,
      vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
      gev::engine::get().queues().compute_family);

    {
      vk::BufferMemoryBarrier2 vbo_barrier;
      vbo_barrier.setBuffer(obj.vertex_buffer().get_buffer())
        .setDstAccessMask(vk::AccessFlagBits2::eShaderStorageRead)
        .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader)
        .setDstQueueFamilyIndex(gev::engine::get().queues().compute_family)
        .setSize(obj.vertex_buffer().size())
        .setSrcStageMask(vk::PipelineStageFlagBits2::eAllCommands)
        .setSrcAccessMask({})
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_EXTERNAL);

      vk::BufferMemoryBarrier2 ibo_barrier;
      ibo_barrier.setBuffer(obj.index_buffer().get_buffer())
        .setDstAccessMask(vk::AccessFlagBits2::eShaderStorageRead)
        .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader)
        .setDstQueueFamilyIndex(gev::engine::get().queues().compute_family)
        .setSize(obj.index_buffer().size())
        .setSrcStageMask(vk::PipelineStageFlagBits2::eAllCommands)
        .setSrcAccessMask({})
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_EXTERNAL);

      vk::DependencyInfo dep;
      dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;
      auto const buffer_barriers = {vbo_barrier, ibo_barrier};
      dep.setBufferMemoryBarriers(buffer_barriers);
      c.pipelineBarrier2(dep);
    }

    c.bindPipeline(vk::PipelineBindPoint::eCompute, _pipeline.get());
    c.bindDescriptorSets(vk::PipelineBindPoint::eCompute, _pipeline_layout.get(), 0, _descriptor, nullptr);
    auto const size = into.image()->extent();
    c.dispatch((size.width + 3) / 4, (size.height + 3) / 4, (size.depth + 3) / 4);

    {
      vk::BufferMemoryBarrier2 vbo_barrier;
      vbo_barrier.setBuffer(obj.vertex_buffer().get_buffer())
        .setSrcAccessMask(vk::AccessFlagBits2::eShaderStorageRead)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eComputeShader)
        .setSrcQueueFamilyIndex(gev::engine::get().queues().compute_family)
        .setSize(obj.vertex_buffer().size())
        .setDstStageMask(vk::PipelineStageFlagBits2::eVertexAttributeInput)
        .setDstAccessMask(vk::AccessFlagBits2::eVertexAttributeRead)
        .setDstQueueFamilyIndex(gev::engine::get().queues().graphics_family);

      vk::BufferMemoryBarrier2 ibo_barrier;
      ibo_barrier.setBuffer(obj.index_buffer().get_buffer())
        .setSrcAccessMask(vk::AccessFlagBits2::eShaderStorageRead)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eComputeShader)
        .setSrcQueueFamilyIndex(gev::engine::get().queues().compute_family)
        .setSize(obj.index_buffer().size())
        .setDstStageMask(vk::PipelineStageFlagBits2::eVertexAttributeInput)
        .setDstAccessMask(vk::AccessFlagBits2::eVertexAttributeRead)
        .setDstQueueFamilyIndex(gev::engine::get().queues().graphics_family);

      vk::DependencyInfo dep;
      dep.dependencyFlags = vk::DependencyFlagBits::eByRegion;
      auto const buffer_barriers = {vbo_barrier, ibo_barrier};
      dep.setBufferMemoryBarriers(buffer_barriers);
      c.pipelineBarrier2(dep);
    }
  }

  void distance_field_generator::pad_bounds(rnu::box3f& box)
  {
    constexpr rnu::vec3 padding = {0.1};

    auto half_size = box.size * 0.5f;
    auto const center = box.position + half_size;

    half_size = padding * half_size + rnu::max(half_size, rnu::vec3(0.05f));

    box.position = center - half_size;
    box.size = 2 * half_size;
  }
}    // namespace gev::game