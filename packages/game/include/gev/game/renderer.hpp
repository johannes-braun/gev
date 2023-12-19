#pragma once

#include <gev/engine.hpp>
#include <gev/image.hpp>
#include <gev/per_frame.hpp>
#include <vulkan/vulkan.hpp>

namespace gev::game
{
  class renderer
  {
  public:
    renderer(vk::Extent2D size, vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);

    void set_render_size(vk::Extent2D size);
    void set_samples(vk::SampleCountFlagBits samples);

    void prepare_frame(frame const& f, bool use_depth = true, bool use_color = true);
    void begin_render(vk::CommandBuffer c, bool use_depth = true, bool use_color = true);
    void bind_pipeline(vk::CommandBuffer c, vk::Pipeline pipeline, vk::PipelineLayout layout);
    void end_render(vk::CommandBuffer c);

    std::shared_ptr<gev::image> color_image(int frame_index);
    std::shared_ptr<gev::image> depth_image(int frame_index);

  private:
    void rebuild_attachments();

    struct render_attachment
    {
      std::shared_ptr<gev::image> image;
      vk::UniqueImageView view;
    };
    gev::per_frame<render_attachment> _per_frame_colors;
    gev::per_frame<render_attachment> _per_frame_depths;

    vk::PipelineLayout _current_pipeline_layout;
    vk::Pipeline _current_pipeline;

    vk::Extent2D _render_size;
    bool _used_depth = false;
    bool _used_color = false;
    vk::RenderingAttachmentInfo _color_attachment;
    vk::RenderingAttachmentInfo _depth_attachment;

    vk::SampleCountFlagBits _samples = vk::SampleCountFlagBits::e1;
  };
}    // namespace gev::game