#include <gev/game/renderer.hpp>
#include <gev/engine.hpp>

namespace gev::game
{
  void renderer::rebuild_attachments()
  {
    gev::engine::get().device().waitIdle();
    _per_frame.reset();

    _per_frame.set_generator([&](int i) {
      auto const size = gev::engine::get().swapchain_size();
      render_attachments idx;
      idx.depth_image = std::make_shared<gev::image>(vk::ImageCreateInfo()
        .setFormat(gev::engine::get().depth_format())
        .setArrayLayers(1)
        .setExtent({ size.width, size.height, 1 })
        .setImageType(vk::ImageType::e2D)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setMipLevels(1)
        .setSamples(_samples)
        .setTiling(vk::ImageTiling::eOptimal)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment));
      idx.depth_view = idx.depth_image->create_view(vk::ImageViewType::e2D);

      if (_samples != vk::SampleCountFlagBits::e1)
      {
        idx.msaa_image = std::make_shared<gev::image>(vk::ImageCreateInfo()
          .setFormat(gev::engine::get().swapchain_format().surfaceFormat.format)
          .setArrayLayers(1)
          .setExtent({ size.width, size.height, 1 })
          .setImageType(vk::ImageType::e2D)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setMipLevels(1)
          .setSamples(_samples)
          .setTiling(vk::ImageTiling::eOptimal)
          .setSharingMode(vk::SharingMode::eExclusive)
          .setUsage(vk::ImageUsageFlagBits::eColorAttachment));
        idx.msaa_view = idx.msaa_image->create_view(vk::ImageViewType::e2D);
      }
      return idx;
      });
  }

  void renderer::set_samples(vk::SampleCountFlagBits samples)
  {
    if (_samples != samples)
    {
      _samples = samples;
      rebuild_attachments();
    }
  }

  renderer::renderer(vk::SampleCountFlagBits samples)
  {
    gev::engine::get().on_resized([this](auto w, auto h) { _per_frame.reset(); });

    set_samples(samples);

    rebuild_attachments();
    _color_attachment = vk::RenderingAttachmentInfo()
      .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
      .setClearValue(vk::ClearValue(std::array{ 0.f, 0.f, 0.f, 0.f }))
      .setStoreOp(vk::AttachmentStoreOp::eStore);

    _depth_attachment = vk::RenderingAttachmentInfo()
      .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
      .setClearValue(vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)))
      .setStoreOp(vk::AttachmentStoreOp::eStore);
  }

  void renderer::prepare_frame(frame const& f)
  {
    auto const& frame = f;
    auto const& index = _per_frame[frame.frame_index];
    auto const& c = frame.command_buffer;

    frame.output_image->layout(c,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
      gev::engine::get().queues().graphics_family);
    index.depth_image->layout(c,
      vk::ImageLayout::eDepthStencilAttachmentOptimal,
      vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      gev::engine::get().queues().graphics_family);

    if (_samples == vk::SampleCountFlagBits::e1)
    {
      _color_attachment
        .setImageView(frame.output_view)
        .setResolveImageView(nullptr)
        .setResolveImageLayout(vk::ImageLayout::eUndefined)
        .setResolveMode(vk::ResolveModeFlagBits::eNone)
        .setLoadOp(vk::AttachmentLoadOp::eClear);
    }
    else
    {
      index.msaa_image->layout(c,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
        gev::engine::get().queues().graphics_family);
      _color_attachment
        .setImageView(index.msaa_view.get())
        .setResolveImageView(frame.output_view)
        .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setResolveMode(vk::ResolveModeFlagBits::eAverage)
        .setLoadOp(vk::AttachmentLoadOp::eClear);
    }

    _depth_attachment
      .setImageView(index.depth_view.get())
      .setLoadOp(vk::AttachmentLoadOp::eClear);

    _current_pipeline_layout = nullptr;
    _current_pipeline = nullptr;
  }

  void renderer::begin_render(frame const& f, bool use_depth)
  {
    _used_depth = use_depth;
    auto const& size = gev::engine::get().swapchain_size();
    auto const& c = f.command_buffer;
    vk::RenderingInfo info{};
    info.setColorAttachments(_color_attachment)
      .setLayerCount(1)
      .setRenderArea(vk::Rect2D({ 0, 0 }, { size.width, size.height }));

    if (use_depth)
      info.setPDepthAttachment(&_depth_attachment);

    c.beginRendering(info);
  }

  void renderer::bind_pipeline(frame const& f, vk::Pipeline pipeline, vk::PipelineLayout layout) {
    auto const& c = f.command_buffer;
    auto const& size = gev::engine::get().swapchain_size();

    _current_pipeline_layout = layout;
    _current_pipeline = pipeline;

    c.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    c.setViewport(0, vk::Viewport(0, 0, size.width, size.height, 0.f, 1.f));
    c.setScissor(0, vk::Rect2D({ 0, 0 }, size));
  }

  void renderer::end_render(frame const& f)
  {
    auto const& c = f.command_buffer;
    _color_attachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
    if (_used_depth)
      _depth_attachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
    c.endRendering();
  }

  vk::PipelineLayout renderer::current_pipeline_layout() const
  {
    return _current_pipeline_layout;
  }

  vk::Pipeline renderer::current_pipeline() const
  {
    return _current_pipeline;
  }
}