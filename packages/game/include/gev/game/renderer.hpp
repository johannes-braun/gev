#pragma once

#include <gev/engine.hpp>
#include <gev/image.hpp>
#include <gev/per_frame.hpp>
#include <vulkan/vulkan.hpp>
#include <rnu/math/math.hpp>

namespace gev::game
{
  class renderer
  {
  public:
    renderer(vk::Extent2D size, vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);

    void set_color_format(vk::Format fmt); 
    void set_depth_format(vk::Format fmt); 
    void set_clear_color(rnu::vec4 color);

    void set_render_size(vk::Extent2D size);
    void set_samples(vk::SampleCountFlagBits samples);

    void prepare_frame(vk::CommandBuffer c, bool use_depth = true, bool use_color = true);
    void begin_render(vk::CommandBuffer c, bool use_depth = true, bool use_color = true);
    void bind_pipeline(vk::CommandBuffer c, vk::Pipeline pipeline, vk::PipelineLayout layout);
    void end_render(vk::CommandBuffer c);

    std::shared_ptr<gev::image> color_image();
    std::shared_ptr<gev::image> depth_image();
    vk::ImageView color_image_view();
    vk::ImageView depth_image_view();
    void set_color_usage(vk::ImageUsageFlags flags);
    void add_color_usage(vk::ImageUsageFlags flags);

  private:
    void rebuild_attachments();
    void force_build_attachments();

    struct render_attachment
    {
      std::shared_ptr<gev::image> image;
      vk::UniqueImageView view;
    };
    render_attachment _color_target;
    render_attachment _depth_target;

    vk::Format _color_format;
    vk::Format _depth_format;

    vk::ImageUsageFlags _color_usage_flags;

    vk::PipelineLayout _current_pipeline_layout;
    vk::Pipeline _current_pipeline;

    vk::Extent2D _render_size;
    bool _used_depth = false;
    bool _used_color = false;
    vk::RenderingAttachmentInfo _color_attachment;
    vk::RenderingAttachmentInfo _depth_attachment;

    vk::SampleCountFlagBits _samples = vk::SampleCountFlagBits::e1;

    bool _rebuild_attachments = true;
  };
}    // namespace gev::game