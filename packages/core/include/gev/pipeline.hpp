#pragma once

#include <vulkan/vulkan.hpp>
#include <filesystem>

namespace gev
{
  std::vector<std::uint32_t> load_spv(std::filesystem::path const& path);
  vk::UniqueShaderModule create_shader(std::span<std::uint32_t const> src);

  vk::UniquePipelineLayout create_pipeline_layout(vk::ArrayProxy<vk::DescriptorSetLayout const> descriptors = {},
    vk::ArrayProxy<vk::PushConstantRange> push_constant_ranges = {});

  vk::UniquePipeline build_compute_pipeline(vk::PipelineLayout layout, vk::ShaderModule module, char const* entry = "main");

  class simple_pipeline_builder
  {
  public:
    static simple_pipeline_builder get(vk::PipelineLayout layout);
    static simple_pipeline_builder get(vk::UniquePipelineLayout const& layout);

    simple_pipeline_builder& dynamic_states(vk::ArrayProxy<vk::DynamicState> states);

    simple_pipeline_builder& stage(vk::ShaderStageFlagBits stage_flags, vk::UniqueShaderModule const& module, vk::SpecializationInfo const& info, std::string name = "main");
    simple_pipeline_builder& stage(vk::ShaderStageFlagBits stage_flags, vk::ShaderModule module, vk::SpecializationInfo const& info, std::string name = "main");
    simple_pipeline_builder& stage(vk::ShaderStageFlagBits stage_flags, vk::UniqueShaderModule const& module, std::string name = "main");
    simple_pipeline_builder& stage(vk::ShaderStageFlagBits stage_flags, vk::ShaderModule module, std::string name = "main");

    simple_pipeline_builder& topology(vk::PrimitiveTopology top);
    simple_pipeline_builder& attribute(std::uint32_t location, std::uint32_t binding, vk::Format format, std::uint32_t offset = 0);
    simple_pipeline_builder& binding(std::uint32_t binding, std::uint32_t stride, vk::VertexInputRate rate = vk::VertexInputRate::eVertex);

    simple_pipeline_builder& multisampling(vk::SampleCountFlagBits samples);
    simple_pipeline_builder& color_attachment(vk::Format format);
    simple_pipeline_builder& depth_attachment(vk::Format format);
    simple_pipeline_builder& stencil_attachment(vk::Format format);

    void clear();
    vk::UniquePipeline build();

  private:
    simple_pipeline_builder() = default;

    std::vector<vk::DynamicState> _dynamic_states = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    std::vector<std::string> _stage_names;
    std::vector<vk::SpecializationInfo> _stage_specializations;
    vk::PipelineLayout _layout;
    std::vector<vk::PipelineShaderStageCreateInfo> _stages;
    std::vector<vk::VertexInputAttributeDescription> _attributes;
    std::vector<vk::VertexInputBindingDescription> _bindings;
    std::vector<vk::Format> _color_formats;
    vk::Format _depth_format = vk::Format::eUndefined;
    vk::Format _stencil_format = vk::Format::eUndefined;
    vk::SampleCountFlagBits _samples = vk::SampleCountFlagBits::e1;
    vk::PrimitiveTopology _topology = vk::PrimitiveTopology::eTriangleList;
  };
}