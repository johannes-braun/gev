#pragma once

#include <gev/image.hpp>
#include <gev/per_frame.hpp>
#include <gev/engine.hpp>
#include <vulkan/vulkan.hpp>

namespace gev::game
{
  class renderer
  {
  public:
    renderer(vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);

    void set_samples(vk::SampleCountFlagBits samples);

    void prepare_frame(frame const& f);
    void begin_render(frame const& f, bool use_depth = true);
    void bind_pipeline(frame const& f, vk::Pipeline pipeline, vk::PipelineLayout layout);
    void end_render(frame const& f);

    vk::PipelineLayout current_pipeline_layout() const;
    vk::Pipeline current_pipeline() const;

  private:
    void rebuild_attachments();

    struct render_attachments
    {
      std::shared_ptr<gev::image> msaa_image;
      vk::UniqueImageView msaa_view;
      std::shared_ptr<gev::image> depth_image;
      vk::UniqueImageView depth_view;
    };
    gev::per_frame<render_attachments> _per_frame;

    vk::PipelineLayout  _current_pipeline_layout;
    vk::Pipeline  _current_pipeline;
    
    bool _used_depth = false;
    vk::RenderingAttachmentInfo _color_attachment;
    vk::RenderingAttachmentInfo _depth_attachment;

    vk::SampleCountFlagBits _samples = vk::SampleCountFlagBits::e1;
  };
}