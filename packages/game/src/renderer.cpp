#include <gev/engine.hpp>
#include <gev/game/renderer.hpp>

namespace gev::game
{
  void renderer::rebuild_attachments()
  {
    _rebuild_attachments = true;
  }

  void renderer::force_build_attachments()
  {
    if (!_rebuild_attachments)
      return;

    _rebuild_attachments = false;

    gev::engine::get().device().waitIdle();
    auto const size = _render_size;
    
    _depth_target.image =
      gev::image_creator::get()
        .size(size.width, size.height)
        .type(vk::ImageType::e2D)
        .samples(_samples)
        .usage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled |
          vk::ImageUsageFlagBits::eTransferSrc)
        .format(_depth_format)
        .build();
    _depth_target.view = _depth_target.image->create_view(vk::ImageViewType::e2D);

    _color_target.image =
      gev::image_creator::get()
        .size(size.width, size.height)
        .type(vk::ImageType::e2D)
        .samples(_samples)
        .usage(_color_usage_flags)
        .format(_color_format)
        .build();
    _color_target.view = _color_target.image->create_view(vk::ImageViewType::e2D);
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
    set_color_usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
      vk::ImageUsageFlagBits::eTransferSrc);

    set_color_format(gev::engine::get().swapchain_format().surfaceFormat.format);
    set_depth_format(gev::engine::get().depth_format());

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

  void renderer::set_color_format(vk::Format fmt)
  {
    if (_color_format != fmt)
    {
      _color_format = fmt;
      rebuild_attachments();
    }
  }

  void renderer::set_depth_format(vk::Format fmt)
  {
    if (_depth_format != fmt)
    {
      _depth_format = fmt;
      rebuild_attachments();
    }
  }

  void renderer::set_clear_color(rnu::vec4 color)
  {
    _color_attachment.clearValue.color.setFloat32({color.r, color.g, color.b, color.a});
  }

  std::shared_ptr<gev::image> renderer::color_image()
  {
    force_build_attachments();
    return _color_target.image;
  }

  std::shared_ptr<gev::image> renderer::depth_image()
  {
    force_build_attachments();
    return _depth_target.image;
  }

  vk::ImageView renderer::color_image_view()
  {
    force_build_attachments();
    return _color_target.view.get();
  }

  vk::ImageView renderer::depth_image_view()
  {
    force_build_attachments();
    return _depth_target.view.get();
  }

  void renderer::set_color_usage(vk::ImageUsageFlags flags)
  {
    if (_color_usage_flags != flags)
    {
      _color_usage_flags = flags;
      rebuild_attachments();
    }
  }

  void renderer::add_color_usage(vk::ImageUsageFlags flags)
  {
    set_color_usage(_color_usage_flags | flags);
  }

  void renderer::prepare_frame(vk::CommandBuffer c, bool use_depth, bool use_color)
  {
    force_build_attachments();

    if (use_depth)
    {
      _depth_target.image->layout(c, vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        gev::engine::get().queues().graphics_family);
      _depth_attachment.setImageView(_depth_target.view.get()).setLoadOp(vk::AttachmentLoadOp::eClear);
    }

    if (use_color)
    {
      _color_target.image->layout(c, vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
        gev::engine::get().queues().graphics_family);

      _color_attachment.setImageView(_color_target.view.get()).setLoadOp(vk::AttachmentLoadOp::eClear);
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