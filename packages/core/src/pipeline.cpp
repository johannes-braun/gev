#include <gev/pipeline.hpp>
#include <gev/engine.hpp>
#include <fstream>

namespace gev
{
  std::vector<std::uint32_t> load_spv(std::filesystem::path const& path)
  {
    std::ifstream file(path, std::ios::binary);

    file.seekg(0, std::ios::end);
    std::size_t const bytes = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::uint32_t> data(bytes / sizeof(std::uint32_t));
    file.read(reinterpret_cast<char*>(data.data()), bytes);
    return data;
  }

  vk::UniqueShaderModule create_shader(std::span<std::uint32_t const> src)
  {
    return engine::get().device().createShaderModuleUnique(vk::ShaderModuleCreateInfo()
      .setCode(src));
  }

  vk::UniquePipelineLayout create_pipeline_layout(vk::ArrayProxy<vk::DescriptorSetLayout const> descriptors,
    vk::ArrayProxy<vk::PushConstantRange> push_constant_ranges)
  {
    return engine::get().device().createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo()
      .setSetLayouts(descriptors)
      .setPushConstantRanges(push_constant_ranges));
  }

  vk::UniquePipeline build_compute_pipeline(vk::PipelineLayout layout, vk::ShaderModule module, char const* entry)
  {
    vk::ComputePipelineCreateInfo ci;
    ci.setStage(vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eCompute, module, entry));
    ci.setLayout(layout);
    return gev::engine::get().device().createComputePipelineUnique(nullptr, ci).value;
  }

  simple_pipeline_builder simple_pipeline_builder::get(vk::PipelineLayout layout)
  {
    simple_pipeline_builder r{};
    r._layout = layout;
    return r;
  }

  simple_pipeline_builder simple_pipeline_builder::get(vk::UniquePipelineLayout const& layout)
  {
    return get(layout.get());
  }

  simple_pipeline_builder& simple_pipeline_builder::dynamic_states(vk::ArrayProxy<vk::DynamicState> states) 
  {
    _dynamic_states.insert(end(_dynamic_states), states.begin(), states.end());
    return *this;
  }

  simple_pipeline_builder& simple_pipeline_builder::stage(vk::ShaderStageFlagBits stage_flags, vk::UniqueShaderModule const& module, vk::SpecializationInfo const& spec, std::string name)
  {
    return stage(stage_flags, module.get(), spec, std::move(name));
  }

  simple_pipeline_builder& simple_pipeline_builder::stage(vk::ShaderStageFlagBits stage_flags, vk::ShaderModule module, vk::SpecializationInfo const& spec, std::string name)
  {
    vk::PipelineShaderStageCreateInfo& info = _stages.emplace_back();
    _stage_names.push_back(std::move(name));
    _stage_specializations.push_back(spec);
    info.setModule(module).setStage(stage_flags).setPSpecializationInfo(std::bit_cast<vk::SpecializationInfo*>(
      _stage_specializations.size()));
    return *this;
  }

  simple_pipeline_builder& simple_pipeline_builder::stage(vk::ShaderStageFlagBits stage_flags, vk::UniqueShaderModule const& module, std::string name)
  {
    return stage(stage_flags, module.get(), std::move(name));
  }

  simple_pipeline_builder& simple_pipeline_builder::stage(vk::ShaderStageFlagBits stage_flags, vk::ShaderModule module, std::string name)
  {
    vk::PipelineShaderStageCreateInfo& info = _stages.emplace_back();
    _stage_names.push_back(std::move(name));
    info.setModule(module).setStage(stage_flags);
      return *this;
  }

  simple_pipeline_builder& simple_pipeline_builder::topology(vk::PrimitiveTopology top)
  {
    _topology = top;
    return *this;
  }

  simple_pipeline_builder& simple_pipeline_builder::attribute(std::uint32_t location, std::uint32_t binding, vk::Format format, std::uint32_t offset)
  {
    _attributes.push_back({ location, binding, format, offset });
    return *this;
  }

  simple_pipeline_builder& simple_pipeline_builder::binding(std::uint32_t binding, std::uint32_t stride, vk::VertexInputRate rate)
  {
    _bindings.push_back({ binding, stride, rate });
    return *this;
  }

  simple_pipeline_builder& simple_pipeline_builder::multisampling(vk::SampleCountFlagBits samples)
  {
    _samples = samples;
    return *this;
  }

  simple_pipeline_builder& simple_pipeline_builder::color_attachment(vk::Format format)
  {
    _color_formats.push_back(format);
    return *this;
  }

  simple_pipeline_builder& simple_pipeline_builder::depth_attachment(vk::Format format)
  {
    _depth_format = format;
    return *this;
  }

  simple_pipeline_builder& simple_pipeline_builder::stencil_attachment(vk::Format format)
  {
    _stencil_format = format;
    return *this;
  }

  void simple_pipeline_builder::clear()
  {
    _stage_names.clear();
    _stages.clear();
    _attributes.clear();
    _bindings.clear();
    _color_formats.clear();
    _depth_format = vk::Format::eUndefined;
    _stencil_format = vk::Format::eUndefined;
  }

  vk::UniquePipeline simple_pipeline_builder::build()
  {
    for (size_t i = 0; i < _stages.size(); ++i)
    {
      _stages[i].pName = _stage_names[i].c_str();

      auto const spec = std::bit_cast<std::size_t>(_stages[i].pSpecializationInfo);
      if (spec == 0)
        continue;

      _stages[i].pSpecializationInfo = &_stage_specializations[spec - 1];
    }

    vk::GraphicsPipelineCreateInfo info;
    info.setLayout(_layout);

    vk::PipelineRenderingCreateInfo rinfo;
    rinfo
      .setColorAttachmentFormats(_color_formats)
      .setDepthAttachmentFormat(_depth_format)
      .setStencilAttachmentFormat(_stencil_format);
    info.pNext = &rinfo;

    vk::PipelineColorBlendStateCreateInfo blend;
    blend.logicOpEnable = false;
    vk::PipelineColorBlendAttachmentState pos_att;
    pos_att.blendEnable = false;
    pos_att.colorWriteMask = vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB;
    auto const attachments = { pos_att };
    blend.setAttachments(attachments);
    info.pColorBlendState = &blend;

    auto const use_depth_test = _depth_format != vk::Format::eUndefined;
    vk::PipelineDepthStencilStateCreateInfo depth;
    depth.stencilTestEnable = false;
    depth.depthTestEnable = use_depth_test;
    depth.depthCompareOp = vk::CompareOp::eLess;
    depth.depthWriteEnable = use_depth_test;
    depth.depthBoundsTestEnable = false;
    info.pDepthStencilState = &depth;

    vk::PipelineDynamicStateCreateInfo dyn;
    dyn.setDynamicStates(_dynamic_states);
    info.pDynamicState = &dyn;

    vk::PipelineInputAssemblyStateCreateInfo input;
    input.primitiveRestartEnable = false;
    input.topology = _topology;
    info.pInputAssemblyState = &input;

    vk::PipelineMultisampleStateCreateInfo msaa;
    msaa.alphaToCoverageEnable = false;
    msaa.sampleShadingEnable = _samples != vk::SampleCountFlagBits::e1;
    msaa.rasterizationSamples = _samples;
    info.pMultisampleState = &msaa;

    vk::PipelineRasterizationStateCreateInfo raster;
    raster.cullMode = vk::CullModeFlagBits::eBack;
    raster.frontFace = vk::FrontFace::eCounterClockwise;
    raster.polygonMode = vk::PolygonMode::eFill;
    raster.rasterizerDiscardEnable = false;
    info.pRasterizationState = &raster;

    info.setStages(_stages);

    info.pTessellationState = nullptr;

    vk::PipelineVertexInputStateCreateInfo vertex;
    vertex.setVertexAttributeDescriptions(_attributes);
    vertex.setVertexBindingDescriptions(_bindings);
    info.pVertexInputState = &vertex;

    vk::PipelineViewportStateCreateInfo vp;
    vk::Viewport viewport;
    vk::Rect2D scissor;
    vp.setViewports(viewport);
    vp.setScissors(scissor);
    info.pViewportState = &vp;

    return engine::get().device().createGraphicsPipelineUnique(nullptr, info).value;
  }
}