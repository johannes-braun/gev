#include "ddf_generator.hpp"
#include <gev/engine.hpp>
#include <gev/pipeline.hpp>
#include <test_shaders_files.hpp>

ddf::ddf(vk::DescriptorSetLayout set_layout, std::unique_ptr<gev::image> image, rnu::box3f bounds)
  : _image(std::move(image)), _bounds(bounds)
{
  _render_descriptor = gev::engine::get().get_descriptor_allocator().allocate(set_layout);

  _bounds_buffer = std::make_unique<gev::buffer>(
    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
    vk::BufferCreateInfo().setSharingMode(vk::SharingMode::eExclusive)
    .setSize(2 * sizeof(rnu::vec4))
    .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
  );

  _bounds_buffer->load_data<rnu::vec4>({ rnu::vec4(bounds.lower(), 1), rnu::vec4(bounds.upper(), 1) });
  _image_view = _image->create_view(vk::ImageViewType::e3D);

  _sampler = gev::engine::get().device().createSamplerUnique(vk::SamplerCreateInfo()
    .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
    .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
    .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
    .setAnisotropyEnable(false)
    .setCompareEnable(false)
    .setMagFilter(vk::Filter::eLinear)
    .setMinFilter(vk::Filter::eLinear)
    .setMipmapMode(vk::SamplerMipmapMode::eNearest)
    .setUnnormalizedCoordinates(false)
    .setMinLod(0)
    .setMaxLod(1000)
  );

  gev::update_descriptor(_render_descriptor, 0, vk::DescriptorBufferInfo()
    .setBuffer(_bounds_buffer->get_buffer())
    .setOffset(0)
    .setRange(_bounds_buffer->size()), vk::DescriptorType::eUniformBuffer);
  gev::update_descriptor(_render_descriptor, 1, vk::DescriptorImageInfo()
    .setImageView(_image_view.get())
    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
    .setSampler(_sampler.get()), vk::DescriptorType::eCombinedImageSampler);
}

std::unique_ptr<gev::image> const& ddf::image() const
{
  return _image;
}

vk::ImageView ddf::image_view() const
{
  return _image_view.get();
}

vk::Sampler ddf::sampler() const
{
  return _sampler.get();
}

rnu::box3f const& ddf::bounds() const
{
  return _bounds;
}

vk::DescriptorSet ddf::render_descriptor() const
{
  return _render_descriptor;
}

struct ddf_options
{
  rnu::vec4 bounds_min;
  rnu::vec4 bounds_max;
  int is_signed = true;
};

ddf_generator::ddf_generator(vk::DescriptorSetLayout camera_set_layout)
{
  _set_layout = gev::descriptor_layout_creator::get()
    .bind(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute)
    .bind(1, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute)
    .bind(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute)
    .bind(3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute)
    .build();
  _pipeline_layout = gev::create_pipeline_layout(_set_layout.get());

  _render_set_layout = gev::descriptor_layout_creator::get()
    .bind(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)
    .bind(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
    .build();
  _render_pipeline_layout = gev::create_pipeline_layout({
    camera_set_layout,
    _render_set_layout.get(),
    });

  _options_buffer = std::make_unique<gev::buffer>(VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
    vk::BufferCreateInfo().setSharingMode(vk::SharingMode::eExclusive)
    .setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
    .setSize(sizeof(ddf_options)));

  _descriptor = gev::engine::get().get_descriptor_allocator().allocate(_set_layout.get());
  gev::update_descriptor(_descriptor, 0, vk::DescriptorBufferInfo(_options_buffer->get_buffer(), 0, _options_buffer->size()),
    vk::DescriptorType::eUniformBuffer);

  recreate_pipelines();
}

void ddf_generator::draw(vk::CommandBuffer c, camera& cam, ddf const& field, int frame)
{
  auto const size = gev::engine::get().swapchain_size();

  c.bindPipeline(vk::PipelineBindPoint::eGraphics, _render_pipeline.get());
  c.setViewport(0, vk::Viewport(0, 0, size.width, size.height, 0.f, 1.f));
  c.setScissor(0, vk::Rect2D({ 0, 0 }, size));
  cam.bind(c, vk::PipelineBindPoint::eGraphics, _render_pipeline_layout.get(), frame);
  c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _render_pipeline_layout.get(), 1, field.render_descriptor(), nullptr);
  c.draw(3, 1, 0, 0);
}

void ddf_generator::recreate_pipelines()
{
  _pipeline.reset();
  _render_pipeline.reset();

  auto const shader = gev::create_shader(gev::load_spv(test_shaders::shaders::generate_distance_field_comp));
  _pipeline = gev::build_compute_pipeline(*_pipeline_layout, *shader);

  auto const vs = gev::create_shader(gev::load_spv(test_shaders::shaders::draw_ddf_vert));
  auto const fs = gev::create_shader(gev::load_spv(test_shaders::shaders::draw_ddf_frag));
  _render_pipeline = gev::simple_pipeline_builder::get(_render_pipeline_layout)
    .stage(vk::ShaderStageFlagBits::eVertex, vs)
    .stage(vk::ShaderStageFlagBits::eFragment, fs)
    .multisampling(vk::SampleCountFlagBits::e4)
    .color_attachment(gev::engine::get().swapchain_format().surfaceFormat.format)
    .depth_attachment(gev::engine::get().depth_format())
    .stencil_attachment(gev::engine::get().depth_format())
    .build();
}

void ddf_generator::pad_bounds(rnu::box3f& box)
{
  auto half_size = box.size * 0.5f;
  auto const center = box.position + half_size;

  half_size = _padding * half_size + rnu::max(half_size, rnu::vec3(0.05f));

  box.position = center - half_size;
  box.size = 2 * half_size;
}

std::unique_ptr<ddf> ddf_generator::generate(object const& obj, std::uint32_t res)
{
  auto bounds = obj.bounds();
  pad_bounds(bounds);

  float max_dim = std::max(bounds.size.x, std::max(bounds.size.y, bounds.size.z));
  float step_size = max_dim / res;

  std::uint32_t sx = std::max(4, (int)std::ceil(bounds.size.x / step_size));
  std::uint32_t sy = std::max(4, (int)std::ceil(bounds.size.y / step_size));
  std::uint32_t sz = std::max(4, (int)std::ceil(bounds.size.z / step_size));

  auto result = std::make_unique<gev::image>(vk::ImageCreateInfo()
    .setFormat(vk::Format::eR16Sfloat)
    .setArrayLayers(1)
    .setExtent({ sx, sy, sz })
    .setImageType(vk::ImageType::e3D)
    .setInitialLayout(vk::ImageLayout::eUndefined)
    .setMipLevels(1)
    .setSamples(vk::SampleCountFlagBits::e1)
    .setTiling(vk::ImageTiling::eOptimal)
    .setSharingMode(vk::SharingMode::eExclusive)
    .setUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled));

  std::unique_ptr<ddf> field = std::make_unique<ddf>(_render_set_layout.get(), std::move(result), bounds);

  generate(*field, obj);

  return field;
}

void ddf_generator::generate(ddf& into, object const& obj)
{
  auto const result_view = into.image()->create_view(vk::ImageViewType::e3D);

  gev::update_descriptor(_descriptor, 1, vk::DescriptorImageInfo()
    .setImageView(result_view.get())
    .setImageLayout(vk::ImageLayout::eGeneral), vk::DescriptorType::eStorageImage);
  gev::update_descriptor(_descriptor, 2, vk::DescriptorBufferInfo()
    .setBuffer(obj.vertex_buffer().get_buffer())
    .setOffset(0)
    .setRange(obj.vertex_buffer().size()), vk::DescriptorType::eStorageBuffer);
  gev::update_descriptor(_descriptor, 3, vk::DescriptorBufferInfo()
    .setBuffer(obj.index_buffer().get_buffer())
    .setOffset(0)
    .setRange(obj.index_buffer().size()), vk::DescriptorType::eStorageBuffer);

  gev::engine::get().execute_once([&](auto c) {
    generate(c, into, result_view.get(), obj);
    }, gev::engine::get().queues().compute,
      gev::engine::get().queues().compute_command_pool.get(), true);
}

void ddf_generator::generate(vk::CommandBuffer c, ddf& into, vk::ImageView view, object const& obj)
{
  auto bounds = obj.bounds();
  pad_bounds(bounds);

  _options_buffer->load_data<ddf_options>(ddf_options{
    .bounds_min = rnu::vec4(bounds.lower(), 1),
    .bounds_max = rnu::vec4(bounds.upper(), 1),
    .is_signed = true
    });

  into.image()->layout(c, vk::ImageLayout::eGeneral,
    vk::PipelineStageFlagBits2::eComputeShader,
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
    auto const buffer_barriers = { vbo_barrier, ibo_barrier };
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
    auto const buffer_barriers = { vbo_barrier, ibo_barrier };
    dep.setBufferMemoryBarriers(buffer_barriers);
    c.pipelineBarrier2(dep);
  }

  into.image()->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eShaderSampledRead,
    gev::engine::get().queues().graphics_family);
}