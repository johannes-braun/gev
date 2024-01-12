#include "environment_shader.hpp"

#include <gev/engine.hpp>
#include <gev/game/layouts.hpp>
#include <gev/game/renderer.hpp>
#include <gev/pipeline.hpp>
#include <test_shaders_files.hpp>

vk::UniquePipelineLayout environment_shader::rebuild_layout()
{
  return gev::create_pipeline_layout(gev::game::layouts::defaults().camera_set_layout());
}

vk::UniquePipeline environment_shader::rebuild(gev::game::pass_id pass)
{
  auto const renderer = gev::service<gev::game::renderer>();
  auto env_vertex_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::environment_vert));
  auto env_fragment_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::environment_frag));
  return gev::simple_pipeline_builder::get(layout())
    .stage(vk::ShaderStageFlagBits::eVertex, env_vertex_shader)
    .stage(vk::ShaderStageFlagBits::eFragment, env_fragment_shader)
    .dynamic_states(vk::DynamicState::eRasterizationSamplesEXT)
    .color_attachment(renderer->get_color_format())
    .build();
}

environment_cube_shader::environment_cube_shader(vk::Format format)
{
  _format = format;
}

vk::UniquePipelineLayout environment_cube_shader::rebuild_layout()
{
  return gev::create_pipeline_layout();
}

vk::UniquePipeline environment_cube_shader::rebuild(gev::game::pass_id pass)
{
  auto env_vertex_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::cube_environment_vert));
  auto env_geometry_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::cube_environment_geom));
  auto env_fragment_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::environment_frag));
  return gev::simple_pipeline_builder::get(layout())
    .stage(vk::ShaderStageFlagBits::eVertex, env_vertex_shader)
    .stage(vk::ShaderStageFlagBits::eGeometry, env_geometry_shader)
    .stage(vk::ShaderStageFlagBits::eFragment, env_fragment_shader)
    .multisampling(vk::SampleCountFlagBits::e1)
    .color_attachment(_format)
    .build();
}

environment_blur_shader::environment_blur_shader(vk::Format format)
{
  _format = format;
  _input_set_layout =
    gev::descriptor_layout_creator::get()
      .flags(vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR)
      .bind(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
      .build();
}

void environment_blur_shader::attach_input(vk::CommandBuffer c, std::shared_ptr<gev::image> image, vk::ImageView view, vk::Sampler sampler) const
{
  vk::DescriptorImageInfo img;
  img.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  img.imageView = view;
  img.sampler = sampler;
  vk::WriteDescriptorSet write;
  write.setDescriptorCount(1).
    setDstArrayElement(0).setDstBinding(0).setDstSet(nullptr).setImageInfo(img)
    .setDescriptorType(vk::DescriptorType::eCombinedImageSampler);

  c.pushDescriptorSetKHR(vk::PipelineBindPoint::eGraphics, layout(), 0, write);
}

vk::UniquePipelineLayout environment_blur_shader::rebuild_layout()
{
  return gev::create_pipeline_layout(*_input_set_layout);
}

vk::UniquePipeline environment_blur_shader::rebuild(gev::game::pass_id pass)
{
  auto env_vertex_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::cube_environment_vert));
  auto env_geometry_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::cube_environment_geom));
  auto env_fragment_shader = gev::create_shader(gev::load_spv(test_shaders::shaders::cube_diffuse_frag));
  return gev::simple_pipeline_builder::get(layout())
    .stage(vk::ShaderStageFlagBits::eVertex, env_vertex_shader)
    .stage(vk::ShaderStageFlagBits::eGeometry, env_geometry_shader)
    .stage(vk::ShaderStageFlagBits::eFragment, env_fragment_shader)
    .multisampling(vk::SampleCountFlagBits::e1)
    .color_attachment(_format)
    .build();
}