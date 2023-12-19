#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

namespace gev::game
{
  enum class pass_id : std::size_t
  {
    forward = 0,
    shadow = 1,

    num_passes
  };

  class shader
  {
  public:
    static std::shared_ptr<shader> make_default();
    static std::shared_ptr<shader> make_skinned();

    shader();

    void invalidate();
    void set_samples(vk::SampleCountFlagBits samples);
    void bind(vk::CommandBuffer c, pass_id pass);
    void attach(vk::CommandBuffer c, vk::DescriptorSet set, std::uint32_t index);
    void attach_always(vk::DescriptorSet set, std::uint32_t index);

    vk::Pipeline pipeline(pass_id pass) const;
    vk::PipelineLayout layout() const;
    vk::SampleCountFlagBits render_samples() const;

  protected:
    virtual vk::UniquePipelineLayout rebuild_layout() = 0;
    virtual vk::UniquePipeline rebuild(pass_id pass) = 0;

  private:
    std::vector<std::pair<std::uint32_t, vk::DescriptorSet>> _global_bindings;
    bool _force_rebuild = false;
    std::vector<vk::UniquePipeline> _pipelines;
    vk::UniquePipelineLayout _layout;
    vk::SampleCountFlagBits _render_samples = vk::SampleCountFlagBits::e1;
  };
}    // namespace gev::game