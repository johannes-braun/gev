#include "environment_shader.hpp"

#include <gev/pipeline.hpp>
#include <gev/game/layouts.hpp>
#include <gev/game/renderer.hpp>
#include <test_shaders_files.hpp>
#include <gev/engine.hpp>

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