#include <gev/engine.hpp>
#include <gev/game/formats.hpp>
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

    _depth_target = std::make_unique<render_target_2d>(size, _depth_format,
      vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled |
        vk::ImageUsageFlagBits::eTransferSrc,
      _samples);
    /*_depth_target.image =
      gev::image_creator::get()
        .size(size.width, size.height)
        .type(vk::ImageType::e2D)
        .samples(_samples)
        .usage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled |
          vk::ImageUsageFlagBits::eTransferSrc)
        .format(_depth_format)
        .build();
    _depth_target.view = _depth_target.image->create_view(vk::ImageViewType::e2D);*/

    _color_target = std::make_unique<render_target_2d>(size, _color_format, _color_usage_flags, _samples);
    /* _color_target.image =
       gev::image_creator::get()
         .size(size.width, size.height)
         .type(vk::ImageType::e2D)
         .samples(_samples)
         .usage(_color_usage_flags)
         .format(_color_format)
         .build();
     _color_target.view = _color_target.image->create_view(vk::ImageViewType::e2D);*/

    _resolve_target.reset();
    if (_samples != vk::SampleCountFlagBits::e1)
    {
      _resolve_target = std::make_unique<render_target_2d>(
        size, _color_format, _color_usage_flags | vk::ImageUsageFlagBits::eTransferDst);
      /* gev::image_creator::get()
         .size(size.width, size.height)
         .type(vk::ImageType::e2D)
         .samples(vk::SampleCountFlagBits::e1)
         .usage(_color_usage_flags | vk::ImageUsageFlagBits::eTransferDst)
         .format(_color_format)
         .build();
     _resolve_target.view = _resolve_target.image->create_view(vk::ImageViewType::e2D);*/
    }
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

    set_color_format(formats::forward_pass);
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

  vk::Format renderer::get_color_format() const
  {
    return _color_format;
  }

  vk::Format renderer::get_depth_format() const
  {
    return _depth_format;
  }

  void renderer::set_clear_color(rnu::vec4 color)
  {
    _color_attachment.clearValue.color.setFloat32({color.r, color.g, color.b, color.a});
  }

  render_target_2d const& renderer::color_target()
  {
    force_build_attachments();
    return *_color_target;
  }
  render_target_2d const& renderer::depth_target()
  {
    force_build_attachments();
    return *_depth_target;
  }
  render_target_2d const& renderer::resolve_target()
  {
    force_build_attachments();
    return _resolve_target ? *_resolve_target : *_color_target;
  }

  // std::shared_ptr<gev::image> renderer::color_image()
  //{
  //   force_build_attachments();
  //   return _color_target.image;
  // }

  // std::shared_ptr<gev::image> renderer::depth_image()
  //{
  //   force_build_attachments();
  //   return _depth_target.image;
  // }

  // vk::ImageView renderer::color_image_view()
  //{
  //   force_build_attachments();
  //   return _color_target.view.get();
  // }

  // vk::ImageView renderer::depth_image_view()
  //{
  //   force_build_attachments();
  //   return _depth_target.view.get();
  // }

  // std::shared_ptr<gev::image> renderer::resolve_image()
  //{
  //   force_build_attachments();
  //   return _resolve_target.image ? _resolve_target.image : _color_target.image;
  // }

  // vk::ImageView renderer::resolve_image_view()
  //{
  //   force_build_attachments();
  //   return _resolve_target.view ? _resolve_target.view.get() : _color_target.view.get();
  // }

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
      _depth_target->image()->layout(c, vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        gev::engine::get().queues().graphics_family);
      _depth_attachment.setImageView(_depth_target->view()).setLoadOp(vk::AttachmentLoadOp::eClear);
    }

    if (use_color)
    {
      _color_target->image()->layout(c, vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
        gev::engine::get().queues().graphics_family);

      _color_attachment.setImageView(_color_target->view()).setLoadOp(vk::AttachmentLoadOp::eClear);
    }
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

  void renderer::resolve(vk::CommandBuffer c)
  {
    if (!_resolve_target)
      return;

    _color_target->image()->layout(
      c, vk::ImageLayout::eTransferSrcOptimal, vk::PipelineStageFlagBits2::eResolve, vk::AccessFlagBits2::eMemoryRead);
    _resolve_target->image()->layout(
      c, vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits2::eResolve, vk::AccessFlagBits2::eMemoryWrite);
    c.resolveImage(_color_target->image()->get_image(), vk::ImageLayout::eTransferSrcOptimal,
      _resolve_target->image()->get_image(), vk::ImageLayout::eTransferDstOptimal,
      vk::ImageResolve()
        .setExtent(vk::Extent3D(_render_size, 1))
        .setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1))
        .setDstSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1)));
  }
}    // namespace gev::game