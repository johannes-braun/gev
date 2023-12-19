#include <gev/engine.hpp>
#include <gev/game/renderer.hpp>

namespace gev::game
{
  void renderer::rebuild_attachments()
  {
    gev::engine::get().device().waitIdle();
    _per_frame_depths.reset();
    _per_frame_colors.reset();

    _per_frame_depths.set_generator(
      [&](int i)
      {
        auto const size = _render_size;
        render_attachment idx;
        idx.image = std::make_shared<gev::image>(
          vk::ImageCreateInfo()
            .setFormat(gev::engine::get().depth_format())
            .setArrayLayers(1)
            .setExtent({size.width, size.height, 1})
            .setImageType(vk::ImageType::e2D)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setMipLevels(1)
            .setSamples(_samples)
            .setTiling(vk::ImageTiling::eOptimal)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled |
              vk::ImageUsageFlagBits::eTransferSrc));
        idx.view = idx.image->create_view(vk::ImageViewType::e2D);
        return idx;
      });

    _per_frame_colors.set_generator(
      [&](int i)
      {
        auto const size = _render_size;
        render_attachment idx;
        idx.image = std::make_shared<gev::image>(
          vk::ImageCreateInfo()
            .setFormat(gev::engine::get().swapchain_format().surfaceFormat.format)
            .setArrayLayers(1)
            .setExtent({size.width, size.height, 1})
            .setImageType(vk::ImageType::e2D)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setMipLevels(1)
            .setSamples(_samples)
            .setTiling(vk::ImageTiling::eOptimal)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
              vk::ImageUsageFlagBits::eTransferSrc));
        idx.view = idx.image->create_view(vk::ImageViewType::e2D);
        return idx;
      });
  }

  void renderer::set_render_size(vk::Extent2D size)
  {
    if (_render_size != size)
    {
      _render_size = size;
      rebuild_attachments();
    }
  }

  void renderer::set_samples(vk::SampleCountFlagBits samples)
  {
    if (_samples != samples)
    {
      _samples = samples;
      rebuild_attachments();
    }
  }

  renderer::renderer(vk::Extent2D size, vk::SampleCountFlagBits samples)
  {
    set_render_size(size);
    set_samples(samples);

    rebuild_attachments();
    _color_attachment =
      vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setClearValue(vk::ClearValue(std::array{0.f, 0.f, 0.f, 0.f}))
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    _depth_attachment =
      vk::RenderingAttachmentInfo()
        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .setClearValue(vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)))
        .setStoreOp(vk::AttachmentStoreOp::eStore);
  }

  std::shared_ptr<gev::image> renderer::color_image(int frame_index)
  {
    auto const& index = _per_frame_colors[frame_index];
    return index.image;
  }

  std::shared_ptr<gev::image> renderer::depth_image(int frame_index)
  {
    auto const& index = _per_frame_depths[frame_index];
    return index.image;
  }

  void renderer::prepare_frame(frame const& f, bool use_depth, bool use_color)
  {
    auto const& frame = f;
    auto const& c = frame.command_buffer;

    if (use_depth)
    {
      auto const& dep = _per_frame_depths[frame.frame_index];
      dep.image->layout(c, vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        gev::engine::get().queues().graphics_family);
      _depth_attachment.setImageView(dep.view.get()).setLoadOp(vk::AttachmentLoadOp::eClear);
    }

    if (use_color)
    {
      auto const& col = _per_frame_colors[frame.frame_index];
      col.image->layout(c, vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
        gev::engine::get().queues().graphics_family);

      _color_attachment.setImageView(col.view.get()).setLoadOp(vk::AttachmentLoadOp::eClear);
    }

    _current_pipeline_layout = nullptr;
    _current_pipeline = nullptr;
  }

  void renderer::begin_render(vk::CommandBuffer c, bool use_depth, bool use_color)
  {
    _used_depth = use_depth;
    _used_color = use_color;
    auto const& size = _render_size;
    vk::RenderingInfo info{};
    info.setLayerCount(1).setRenderArea(vk::Rect2D({0, 0}, {size.width, size.height}));

    if (use_color)
      info.setColorAttachments(_color_attachment);

    if (use_depth)
      info.setPDepthAttachment(&_depth_attachment);

    c.beginRendering(info);
  }

  void renderer::bind_pipeline(vk::CommandBuffer c, vk::Pipeline pipeline, vk::PipelineLayout layout)
  {
    auto const& size = _render_size;

    _current_pipeline_layout = layout;
    _current_pipeline = pipeline;

    c.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    c.setViewport(0, vk::Viewport(0, 0, size.width, size.height, 0.f, 1.f));
    c.setScissor(0, vk::Rect2D({0, 0}, size));
  }

  void renderer::end_render(vk::CommandBuffer c)
  {
    if (_used_color)
      _color_attachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
    if (_used_depth)
      _depth_attachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
    c.endRendering();
  }
}    // namespace gev::game