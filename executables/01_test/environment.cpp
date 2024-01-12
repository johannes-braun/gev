#include "environment.hpp"

#include <gev/game/samplers.hpp>
#include <gev/game/camera.hpp>
#include <gev/game/layouts.hpp>

environment::environment(vk::Format cube_format) : _format(cube_format)
{
  _forward_shader = std::make_shared<environment_shader>();
  _cube_shader = std::make_shared<environment_cube_shader>(_format);
  _blur_shader = std::make_shared<environment_blur_shader>(_format);

  vk::ImageCreateInfo image_info;
  image_info.setArrayLayers(6)
    .setExtent(vk::Extent3D(diffuse_size, diffuse_size, 1))
    .setFormat(vk::Format::eB10G11R11UfloatPack32)
    .setImageType(vk::ImageType::e2D)
    .setFlags(vk::ImageCreateFlagBits::eCubeCompatible)
    .setInitialLayout(vk::ImageLayout::eUndefined)
    .setMipLevels(1)
    .setSharingMode(vk::SharingMode::eExclusive)
    .setSamples(vk::SampleCountFlagBits::e1)
    .setTiling(vk::ImageTiling::eOptimal)
    .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
  _cubemap = std::make_shared<gev::image>(image_info);
  _cubemap_view = _cubemap->create_view(vk::ImageViewType::eCube);
  _diffuse_cubemap = std::make_shared<gev::image>(image_info);
  _diffuse_cubemap_view = _diffuse_cubemap->create_view(vk::ImageViewType::eCube);

  _environment_set =
    gev::engine::get().get_descriptor_allocator().allocate(gev::game::layouts::defaults().environment_set_layout());
  gev::update_descriptor(_environment_set, 0,
    vk::DescriptorImageInfo()
      .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
      .setImageView(_diffuse_cubemap_view.get())
      .setSampler(gev::game::samplers::defaults().cubemap()),
    vk::DescriptorType::eCombinedImageSampler);

  auto mesh_renderer = gev::service<gev::game::mesh_renderer>();
  mesh_renderer->set_environment_map(_environment_set);
}

void environment::render_cube(vk::CommandBuffer c)
{
  constexpr std::uint32_t w = diffuse_size;
  constexpr std::uint32_t h = diffuse_size;

  _cubemap->layout(c, vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::AccessFlagBits2::eColorAttachmentWrite);
  vk::RenderingAttachmentInfo rimg;
  rimg.setClearValue(vk::ClearColorValue({0.0f, 0.0f, 0.0f, 1.0f}));
  rimg.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
    .setImageView(_cubemap_view.get())
    .setLoadOp(vk::AttachmentLoadOp::eClear)
    .setStoreOp(vk::AttachmentStoreOp::eStore);

  vk::RenderingInfo ri;
  ri.setLayerCount(6).setRenderArea(vk::Rect2D({0, 0}, {w, h}));
  ri.setColorAttachments(rimg);

  c.beginRendering(ri);
  _cube_shader->bind(c, gev::game::pass_id::forward);
  c.setViewport(0, vk::Viewport(0.0f, 0.0f, w, h, 0.0f, 1.0f));
  c.setScissor(0, vk::Rect2D({0, 0}, {w, h}));
  c.draw(3, 1, 0, 0);
  c.endRendering();

  _diffuse_cubemap->layout(c, vk::ImageLayout::eColorAttachmentOptimal,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite);
  _cubemap->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eShaderSampledRead);

  auto const sampler = gev::game::samplers::defaults().cubemap();
  rimg.imageView = _diffuse_cubemap_view.get();
  c.beginRendering(ri);
  _blur_shader->bind(c, gev::game::pass_id::forward);
  _blur_shader->attach_input(c, _cubemap, _cubemap_view.get(), sampler);
  c.setViewport(0, vk::Viewport(0.0f, 0.0f, w, h, 0.0f, 1.0f));
  c.setScissor(0, vk::Rect2D({0, 0}, {w, h}));
  c.draw(3, 1, 0, 0);
  c.endRendering();

  _diffuse_cubemap->layout(c, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eShaderSampledRead);
}

void environment::render(vk::CommandBuffer c, std::int32_t x, std::int32_t y, std::uint32_t w, std::uint32_t h)
{
  _renderer->begin_render(c, false);
  _forward_shader->bind(c, gev::game::pass_id::forward);
  c.setViewport(0, vk::Viewport(float(x), float(y), float(w), float(h), 0.f, 1.f));
  c.setScissor(0, vk::Rect2D({x, y}, {w, h}));
  c.setRasterizationSamplesEXT(_renderer->get_samples());
  _forward_shader->attach(c, _controls->main_camera->descriptor(), gev::game::mesh_renderer::camera_set);
  c.draw(3, 1, 0, 0);
  _renderer->end_render(c);

  render_cube(c);
}
